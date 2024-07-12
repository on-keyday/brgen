use anyhow::{anyhow, Result};
use ast2rust::ast::{GenerateMapFile, StructType, StructUnionType};
use ast2rust::eval::topological_sort_format;
use ast2rust::{ast, PtrKey};
use ast2rust_macro::{ptr, ptr_null};
use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;

type SharedPtr<T> = Rc<RefCell<T>>;

struct AnonymousStruct {
    name: String,
}

pub struct Generator<W: std::io::Write> {
    w: Writer<W>,
    seq: usize,
    encode: bool,

    pub map_file: GenerateMapFile,

    structs: HashMap<PtrKey<StructType>, AnonymousStruct>,
    struct_unions: HashMap<PtrKey<StructUnionType>, AnonymousStruct>,
}

pub struct Writer<W: std::io::Write> {
    pub writer: W,
    indent: RefCell<usize>,

    should_indent: bool,
}

struct IndentScope<'a> {
    indent: &'a RefCell<usize>,
}

impl Drop for IndentScope<'_> {
    fn drop(&mut self) {
        *self.indent.borrow_mut() -= 1;
    }
}

impl<'a, W: std::io::Write> Writer<W> {
    pub fn new(w: W) -> Self {
        Self {
            writer: w,
            indent: RefCell::new(0),
            should_indent: false,
        }
    }

    pub fn flush(&mut self) -> Result<()> {
        self.writer.flush()?;
        Ok(())
    }

    pub fn get_mut_writer(&mut self) -> &mut W {
        &mut self.writer
    }

    fn enter_indent_scope(&self) -> IndentScope<'a> {
        *self.indent.borrow_mut() += 1;
        IndentScope {
            // to escape from the lifetime checker,
            // we need to cast the reference to a raw pointer
            // and then back to a reference.
            // so we can create a reference with a new lifetime.
            indent: unsafe { &*(&self.indent as *const _) },
        }
    }

    fn write_indent(&mut self) -> Result<()> {
        for _ in 0..*self.indent.borrow() {
            self.writer.write(b"    ")?;
        }
        Ok(())
    }

    fn write(&mut self, s: &str) -> Result<()> {
        self.write_if_should_indent()?;
        self.writer.write(s.as_bytes())?;
        Ok(())
    }

    fn write_if_should_indent(&mut self) -> Result<()> {
        if self.should_indent {
            self.write_indent()?;
            self.should_indent = false;
        }
        Ok(())
    }

    fn writeln(&mut self, s: &str) -> Result<()> {
        self.write(s)?;
        self.write("\n")?;
        self.should_indent = true;
        Ok(())
    }
}

impl<W: std::io::Write> Generator<W> {
    pub fn new(w: W) -> Self {
        Self {
            w: Writer::new(w),
            seq: 0,
            encode: false,
            structs: HashMap::new(),
            struct_unions: HashMap::new(),
            map_file: GenerateMapFile {
                structs: Vec::new(),
                line_map: Vec::new(),
            },
        }
    }

    fn get_seq(&mut self) -> usize {
        let seq = self.seq;
        self.seq += 1;
        seq
    }

    pub fn get_mut_writer(&mut self) -> &mut W {
        self.w.get_mut_writer()
    }

    pub fn get_type(&mut self, typ: &ast::Type) -> Result<String> {
        match typ {
            ast::Type::IntType(t) => {
                if ptr_null!(t.is_common_supported) {
                    if ptr_null!(t.is_signed) {
                        Ok(format!("i{}", ptr!(t.bit_size)))
                    } else {
                        Ok(format!("u{}", ptr!(t.bit_size)))
                    }
                } else {
                    let v = ptr!(t.bit_size);
                    if v < 64 {
                        let aligned = (v + 7) / 8;
                        if t.borrow().is_signed {
                            Ok(format!("i{}", aligned * 8))
                        } else {
                            Ok(format!("u{}", aligned * 8))
                        }
                    } else {
                        Ok(format!("i{}", v))
                    }
                }
            }
            ast::Type::BoolType(_) => Ok("bool".to_string()),
            ast::Type::EnumType(t) => {
                let x = ptr_null!(t.base->ident.ident);
                Ok(x)
            }
            ast::Type::StructType(t) => {
                let struct_ = t.borrow().base.clone().unwrap();
                match struct_ {
                    ast::NodeWeak::Format(struct_) => {
                        let x = ptr_null!(struct_->ident.ident);
                        Ok(x)
                    }
                    x => {
                        eprintln!("{:?}", ast::NodeType::try_from(x));
                        todo!("unsupported")
                    }
                }
            }
            ast::Type::IdentType(t) => {
                let ident = ptr_null!(t.ident.ident);
                Ok(ident)
            }
            ast::Type::ArrayType(t) => {
                let ty = ptr!(t.element_type);
                let ty = self.get_type(&ty)?;
                if t.borrow().length_value.is_some() {
                    Ok(format!("[{}; {}]", ty, ptr!(t.length_value)))
                } else {
                    Ok(format!("Vec<{}>", ty))
                }
            }
            ast::Type::StructUnionType(x) => {
                if let Some(x) = self.struct_unions.get(&PtrKey::new(x)) {
                    Ok(x.name.clone())
                } else {
                    let name = format!("AnonymousStructUnion{}", self.get_seq());
                    let mut v = Vec::new();
                    for structs in ptr_null!(x.structs).iter() {
                        let name = format!("AnonymousStruct{}", self.get_seq());
                        let mut tmp_w = Writer::new(Vec::new());
                        tmp_w.write("pub struct ")?;
                        tmp_w.write(&name)?;
                        tmp_w.writeln(" {")?;
                        self.write_struct_type(&mut tmp_w, structs.clone())?;
                        tmp_w.writeln("}")?;
                        self.w.write(&String::from_utf8(tmp_w.writer)?)?;
                        self.structs
                            .insert(PtrKey::new(structs), AnonymousStruct { name: name.clone() });
                        v.push(name);
                    }
                    let mut tmp_w = Writer::new(Vec::new());
                    tmp_w.write("pub enum ")?;
                    tmp_w.write(&name)?;
                    tmp_w.writeln(" {")?;
                    for name in v.iter() {
                        tmp_w.write(&name)?;
                        tmp_w.write("(")?;
                        tmp_w.write(&name)?;
                        tmp_w.writeln("),")?;
                    }
                    tmp_w.writeln("}")?;
                    self.w.write(std::str::from_utf8(&tmp_w.writer)?)?;
                    Ok(name)
                }
            }
            x => {
                eprintln!("{:?}", ast::NodeType::from(ast::Node::from(x)));
                Err(anyhow!("unsupported"))
            }
        }
    }

    pub fn write_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
    ) -> Result<()> {
        if let ast::Type::UnionType(_) = ptr!(field.field_type) {
            return Ok(());
        }
        if field.borrow().ident.is_none() {
            let some = Some(Rc::new(RefCell::new(ast::Ident {
                loc: field.borrow().loc.clone(),
                expr_type: ptr_null!(field.field_type),
                ident: format!("hidden_field_{}", self.get_seq()),
                base: Some(ast::NodeWeak::Field(Rc::downgrade(&field))),
                constant_level: ast::ConstantLevel::Variable,
                usage: ast::IdentUsage::DefineField,
                scope: None,
            })));
            field.borrow_mut().ident = some;
        }
        w.write("pub ")?;
        w.write(&format!("{}: ", ptr_null!(field.ident.ident)))?;
        let typ = self.get_type(&ptr!(field.field_type))?;
        w.writeln(&format!("{},", typ))?;
        Ok(())
    }

    pub fn write_struct_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        ty: SharedPtr<ast::StructType>,
    ) -> Result<()> {
        let ty = ty.borrow();
        for field in ty.fields.iter() {
            if let ast::Member::Field(field) = field {
                self.write_field(w, field)?;
            }
        }
        Ok(())
    }

    pub fn write_encode_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
    ) -> Result<()> {
        let ident = ptr_null!(field.ident.ident);
        w.writeln(&format!(
            "serde::ser::SerializeStruct::serialize_field(&mut serializer, \"{}\", &self.{})?;",
            ident, ident
        ))?;
        Ok(())
    }

    pub fn write_decode_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
    ) -> Result<()> {
        let ident = ptr_null!(field.ident.ident);
        let ty = self.get_type(&ptr!(field.field_type))?;
        w.writeln(&format!("let {} = map.next_value::<{}>();", ident, ty))?;
        Ok(())
    }

    pub fn write_format_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        fmt: SharedPtr<ast::Format>,
    ) -> Result<()> {
        let ident = ptr_null!(fmt.ident.ident);
        self.map_file.structs.push(ident.clone());
        w.writeln(&format!("pub struct {} {{", ident))?;
        {
            let _scope = w.enter_indent_scope();
            self.write_struct_type(w, ptr!(fmt.body.struct_type))?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_node<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        node: ast::Node,
    ) -> Result<()> {
        match node {
            ast::Node::IndentBlock(block) => {
                for elem in ptr_null!(block.elements).iter() {
                    self.write_node(w, elem.clone())?;
                }
            }
            ast::Node::Field(field) => {
                if self.encode {
                    self.write_encode_field(w, &field)?;
                } else {
                    self.write_decode_field(w, &field)?;
                }
            }
            _ => {} // TODO: Implement,skip for now
        }
        Ok(())
    }

    pub fn write_encode_fn<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        ty: SharedPtr<ast::Format>,
    ) -> Result<()> {
        let ident = ptr_null!(ty.ident.ident);
        w.writeln(&format!("impl serde::ser::Serialize for {} {{", ident))?;
        {
            let _scope = w.enter_indent_scope();
            w.writeln("fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::ser::Serializer {")?;
            {
                let _scope = w.enter_indent_scope();
                self.write_node(w, ast::Node::IndentBlock(ptr!(ty.body)))?;
                w.writeln("serializer.end()")?;
                w.writeln("}")?;
            }
            _ = _scope;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_decode_fn<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        ty: SharedPtr<ast::Format>,
    ) -> Result<()> {
        let ident = ptr_null!(ty.ident.ident);
        w.writeln(&format!(
            "impl<'de> serde::de::Deserialize<'de> for {} {{",
            ident
        ))?;
        {
            let _scope = w.enter_indent_scope();
            w.writeln(&format!("fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::de::Deserializer<'de> {{"))?;
            {
                let _scope = w.enter_indent_scope();
                w.writeln("struct Visitor;")?;
                w.writeln("impl<'de> serde::de::Visitor<'de> for Visitor {")?;
                {
                    let _scope = w.enter_indent_scope();
                    w.writeln("type Value = Self;")?;
                    w.writeln("fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {")?;
                    {
                        let _scope = w.enter_indent_scope();
                        w.writeln("formatter.write_str(\"struct\")")?;
                    }
                    w.writeln("}")?;
                    w.writeln("fn visit_map<A>(self, mut map: A) -> Result<Self::Value, A::Error> where A: serde::de::MapAccess<'de> {")?;
                    {
                        let _scope = w.enter_indent_scope();
                        self.write_node(w, ast::Node::IndentBlock(ptr!(ty.body)))?;
                        w.writeln("Ok(self)")?;
                    }
                    w.writeln("}")?;
                }
                w.writeln("}")?;
                w.writeln("deserializer.deserialize_map(Visitor)")?;
            }
            w.writeln("}")?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_format_fns<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        fmt: SharedPtr<ast::Format>,
    ) -> Result<()> {
        //SAFETY: We are casting a mutable reference to a mutable reference, which is safe.
        self.encode = true;
        self.write_encode_fn(w, fmt.clone())?;
        self.encode = false;
        self.write_decode_fn(w, fmt.clone())?;
        Ok(())
    }

    pub fn write_enum(&mut self, enum_: SharedPtr<ast::Enum>) -> Result<()> {
        let ident = ptr_null!(enum_.ident.ident);
        self.w.writeln(&format!("pub enum {} {{", ident))?;
        {
            let _scope = self.w.enter_indent_scope();
            for elem in ptr_null!(enum_.members).iter() {
                self.w
                    .writeln(&format!("{},", ptr_null!(elem.ident.ident)))?;
            }
        }
        self.w.writeln("}")?;
        Ok(())
    }

    pub fn write_program(&mut self, in_prog: SharedPtr<ast::Program>) -> Result<()> {
        let prog = in_prog.borrow();
        self.w.writeln("use serde;")?;
        for elem in prog.elements.iter() {
            if let Ok(enum_) = elem.try_into() {
                self.write_enum(enum_)?;
            }
        }
        let sorted = topological_sort_format(&in_prog).ok_or(anyhow!("error"))?;
        let mut w = Writer::new(Vec::new());
        for elem in sorted.iter() {
            self.write_format_type(&mut w, elem.clone())?;
        }
        for elem in sorted {
            self.write_format_fns(&mut w, elem)?;
        }
        self.w.write(&String::from_utf8(w.writer)?)?;
        self.w.flush()?;
        Ok(())
    }
}
