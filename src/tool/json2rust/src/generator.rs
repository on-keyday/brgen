use anyhow::{anyhow, Result};
use ast2rust::ast::{GenerateMapFile, StructType, StructUnionType, UnionType};
use ast2rust::eval::{topological_sort_format, Stringer};
use ast2rust::{ast, PtrKey, PtrUnwrapError};
use ast2rust_macro::{ptr, ptr_null};
use std::ascii::escape_default;
use std::cell::RefCell;
use std::collections::HashMap;
use std::f32::consts::E;
use std::rc::Rc;

type SharedPtr<T> = Rc<RefCell<T>>;

struct AnonymousStruct {
    name: String,
}

struct BitFields {
    fields: Vec<SharedPtr<ast::Field>>,
}

pub struct Generator<W: std::io::Write> {
    w: Writer<W>,
    seq: usize,
    encode: bool,

    pub map_file: GenerateMapFile,

    structs: HashMap<PtrKey<StructType>, AnonymousStruct>,
    struct_unions: HashMap<PtrKey<StructUnionType>, AnonymousStruct>,
    s: Stringer,
}

pub struct Writer<W: std::io::Write> {
    pub writer: W,
    indent: RefCell<usize>,

    should_indent: bool,

    pub prefix: String,
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
            prefix: String::new(),
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
            s: Stringer::new("self".into()),
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

    pub fn expr_to_string(&self, x: &ast::Expr) -> Result<String> {
        return self.s.to_string(x).map_err(|x| anyhow!("{:?}", x));
    }

    pub fn get_int_type(
        &mut self,
        bit_size: u64,
        is_signed: bool,
        is_common_supported: bool,
    ) -> Result<String> {
        if is_common_supported {
            if is_signed {
                Ok(format!("i{}", bit_size))
            } else {
                Ok(format!("u{}", bit_size))
            }
        } else {
            let v = bit_size;
            if v < 64 {
                let aligned = (v + 7) / 8;
                if is_signed {
                    Ok(format!("i{}", aligned * 8))
                } else {
                    Ok(format!("u{}", aligned * 8))
                }
            } else {
                Ok(format!("i{}", v))
            }
        }
    }

    pub fn is_primitive_array(t: &SharedPtr<ast::ArrayType>) -> bool {
        match t.borrow().element_type.as_ref() {
            Some(ast::Type::IntType(_)) => ptr_null!(t.length_value).is_some(),
            _ => false,
        }
    }

    pub fn gen_anonymous_union<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        x: &SharedPtr<StructUnionType>,
    ) -> Result<String> {
        let name = format!("AnonymousStructUnion{}", self.get_seq());
        let mut v = Vec::new();
        for structs in ptr_null!(x.structs).iter() {
            let name_s = format!("AnonymousStruct{}", self.get_seq());
            let mut tmp_w = Writer::new(Vec::new());
            let prefix = &w.prefix;
            tmp_w.prefix =
                format!("(match {prefix} {{ {name}::{name_s}(x) => x, _ => return $ERROR }})?");
            tmp_w.writeln("#[derive(Default,Debug,Clone,PartialEq)]")?;
            tmp_w.write("pub struct ")?;
            tmp_w.write(&name_s)?;
            tmp_w.writeln(" {")?;
            self.write_struct_type(&mut tmp_w, structs.clone())?;
            tmp_w.writeln("}")?;
            self.w.write(&String::from_utf8(tmp_w.writer)?)?;
            self.structs.insert(
                PtrKey::new(structs),
                AnonymousStruct {
                    name: name_s.clone(),
                },
            );
            v.push(name_s);
        }
        let mut tmp_w = Writer::new(Vec::new());
        tmp_w.writeln("#[derive(Default,Debug,Clone,PartialEq)]")?;
        tmp_w.write("pub enum ")?;
        tmp_w.write(&name)?;
        tmp_w.writeln(" {")?;
        tmp_w.writeln("#[default]")?;
        tmp_w.writeln("None,")?;
        for name in v.iter() {
            tmp_w.write(&name)?;
            tmp_w.write("(")?;
            tmp_w.write(&name)?;
            tmp_w.writeln("),")?;
        }
        tmp_w.writeln("}")?;

        let union_fields = ptr_null!(x.union_fields);
        for field in union_fields.iter() {
            let typ: SharedPtr<UnionType> = ptr!(field->field_type)
                .try_into()
                .map_err(|x| anyhow!("{:?}", x))?;
            let cond0 = ptr_null!(typ.cond);
            let cond0 = if let Some(x) = cond0 {
                x.upgrade()
            } else {
                None
            };
            let c = ptr_null!(typ.common_type);
            if let Some(c) = c {
                let c = self.get_type(&c)?;
                tmp_w.writeln(&format!("impl {name} {{"))?;
                {
                    let _scope = tmp_w.enter_indent_scope();
                    tmp_w.writeln(&format!(
                        "pub fn {}(&self) -> Option<{}> {{",
                        ptr_null!(field->ident.ident),
                        c
                    ))?;
                    {
                        let _scope = tmp_w.enter_indent_scope();

                        for cand in ptr_null!(typ.candidates) {
                            let cond = ptr!(cand.cond).upgrade().ok_or_else(|| anyhow!("error"))?;
                            let cond = if let Some(cond0) = &cond0 {
                                self.s
                                    .eval_ephemeral_binary_op(ast::BinaryOp::Equal, &cond, cond0)
                                    .map_err(|x| anyhow!("{:?}", x))?
                            } else {
                                self.expr_to_string(&cond)?
                            };

                            tmp_w.writeln(&format!("if {cond} {{ "))?;
                            {
                                let _scope = tmp_w.enter_indent_scope();
                                if let Some(field) = field.upgrade() {
                                    let ident = self.expr_to_string(&ptr!(field.ident).into())?;
                                    tmp_w.writeln(&format!("return Some({}),", ident))?;
                                } else {
                                    tmp_w.writeln("return None")?;
                                }
                            }
                            tmp_w.writeln("}")?;
                        }

                        tmp_w.writeln("None")?;
                    }
                    tmp_w.writeln("}")?;

                    tmp_w.writeln(&format!(
                        "pub fn set_{}(&mut self, x: {}) -> bool {{",
                        ptr_null!(field->ident.ident),
                        c
                    ))?;
                    {
                        let _scope = tmp_w.enter_indent_scope();
                        tmp_w.writeln("match self {")?;
                        {
                            let _scope = tmp_w.enter_indent_scope();
                            for name in v.iter() {
                                tmp_w.writeln(&format!(
                                    "{}::{}(y) => *y.{} = x,",
                                    name,
                                    name,
                                    ptr_null!(field->ident.ident)
                                ))?;
                            }
                        }
                        tmp_w.writeln("}")?;
                    }
                }
            }
        }

        self.w.write(std::str::from_utf8(&tmp_w.writer)?)?;
        Ok(name)
    }

    pub fn get_type(&mut self, typ: &ast::Type) -> Result<String> {
        match typ {
            ast::Type::IntType(t) => {
                let x = self.get_int_type(
                    ptr!(t.bit_size),
                    ptr_null!(t.is_signed),
                    ptr_null!(t.is_common_supported),
                )?;
                Ok(x)
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
                        todo!("unsupported struct type")
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
                if Self::is_primitive_array(t) {
                    Ok(format!("[{}; {}]", ty, ptr!(t.length_value)))
                } else {
                    Ok(format!("Vec<{}>", ty))
                }
            }
            ast::Type::StructUnionType(x) => {
                if let Some(x) = self.struct_unions.get(&PtrKey::new(x)) {
                    return Ok(x.name.clone());
                } else {
                    return Err(anyhow!("unsupported type"));
                }
            }
            x => {
                eprintln!("{:?}", ast::NodeType::from(ast::Node::from(x)));
                Err(anyhow!("unsupported type"))
            }
        }
    }

    pub fn write_bit_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        bit_fields: &Vec<SharedPtr<ast::Field>>,
    ) -> Result<()> {
        let sum: u64 = bit_fields
            .iter()
            .try_fold(0 as u64, |acc, x| {
                Some(acc + x.borrow().field_type.as_ref()?.get_bit_size()?)
            })
            .ok_or_else(|| anyhow!("error"))?;
        if sum % 8 != 0 {
            return Err(anyhow!("error"));
        }

        Ok(())
    }

    pub fn escape_keyword(s: String) -> String {
        match s.as_str() {
            "type" => "type_".to_string(),
            _ => s,
        }
    }

    pub fn write_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
        bit_fields: &mut Vec<SharedPtr<ast::Field>>,
    ) -> Result<()> {
        if ptr_null!(field.bit_alignment) != ptr_null!(field.eventual_bit_alignment) {
            bit_fields.push(field.clone());
            return Ok(());
        }
        if bit_fields.len() > 0 {
            return self.write_bit_field(w, bit_fields);
        }
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
        if let ast::Type::StructUnionType(x) = ptr!(field.field_type) {
            self.gen_anonymous_union(w, &x)?;
        }
        w.write("pub ")?;
        let ident = ptr_null!(field.ident.ident);
        let ident = Self::escape_keyword(ident);
        w.write(&format!("{}: ", ident))?;
        let typ = self.get_type(&ptr!(field.field_type))?;
        w.writeln(&format!("{},", typ))?;
        let prefix = &w.prefix;
        self.s
            .add_map(&ptr!(field.ident), &format!("{prefix}.{ident}"));
        Ok(())
    }

    pub fn write_struct_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        ty: SharedPtr<ast::StructType>,
    ) -> Result<()> {
        let ty = ty.borrow();
        let mut bit_fields = Vec::new();
        for field in ty.fields.iter() {
            if let ast::Member::Field(field) = field {
                self.write_field(w, field, &mut bit_fields)?;
            }
        }
        Ok(())
    }

    pub fn write_encode_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &ast::Field,
        typ: ast::Type,
        ident: &str,
    ) -> Result<()> {
        match typ {
            ast::Type::IntType(ity) => {
                if ptr_null!(ity.is_common_supported) {
                    match ptr_null!(ity.endian) {
                        ast::Endian::Big | ast::Endian::Unspec => {
                            w.writeln(&format!("w.write(&{}.to_be_bytes())?;", ident))?;
                        }
                        ast::Endian::Little => {
                            w.writeln(&format!("w.write(&{}.to_le_bytes())?;", ident))?;
                        }
                    }
                }
                return Ok(());
            }
            ast::Type::IdentType(t) => {
                let base = ptr!(t.base)
                    .try_into()
                    .map_err(|x: ast::Error| anyhow!("{:?}", x))?;
                self.write_encode_type(w, field, base, ident)?;
                return Ok(());
            }
            ast::Type::EnumType(t) => {
                let base = ptr!(t.base->base_type);
                self.write_encode_type(w, field, base, ident)?;
                return Ok(());
            }
            ast::Type::ArrayType(t) => {
                if Self::is_primitive_array(&t) {
                    w.writeln(&format!("for i in 0..{} {{", ptr!(t.length_value)))?;
                    {
                        let _scope = w.enter_indent_scope();
                        self.write_encode_type(
                            w,
                            field,
                            ptr!(t.element_type),
                            &format!("{}[i]", ident),
                        )?;
                    }
                    w.writeln("}")?;
                } else {
                    let len = match ptr_null!(t.length_value) {
                        None => self.expr_to_string(&ptr!(t.length))?,
                        Some(x) => x.to_string(),
                    };
                    w.writeln(&format!("if {} != {}.len() {{", len, ident))?;
                    {
                        let _scope = w.enter_indent_scope();
                        w.writeln(&format!("return Err(anyhow!(\"error\"));"))?;
                    }
                    w.writeln(&format!("for i in 0..{}.len() {{", ident))?;
                    {
                        let _scope = w.enter_indent_scope();
                        self.write_encode_type(
                            w,
                            field,
                            ptr!(t.element_type),
                            &format!("{}[i]", ident),
                        )?;
                    }
                    w.writeln("}")?;
                }
                return Ok(());
            }
            _ => {}
        }
        Err(anyhow!("unsupported encode type"))
    }

    pub fn write_decode_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &ast::Field,
        typ: ast::Type,
        ident: &str,
    ) -> Result<()> {
        match typ {
            ast::Type::IntType(ity) => {
                if ptr_null!(ity.is_common_supported) {
                    let tmp = format!("tmp{}", self.get_seq());
                    let len = ptr!(ity.bit_size) / 8;
                    w.writeln(&format!("let mut {} = [0; {}];", tmp, len))?;
                    w.writeln(&format!("r.read(&mut {})?;", tmp))?;
                    match ptr_null!(ity.endian) {
                        ast::Endian::Big | ast::Endian::Unspec => {
                            w.writeln(&format!(
                                "{} = u{}::from_be_bytes({});",
                                ident,
                                ptr!(ity.bit_size),
                                tmp,
                            ))?;
                        }
                        ast::Endian::Little => {
                            w.writeln(&format!(
                                "{} = u{}::from_le_bytes({});",
                                ident,
                                ptr!(ity.bit_size),
                                tmp,
                            ))?;
                        }
                    }
                }
                return Ok(());
            }
            ast::Type::IdentType(t) => {
                let base = ptr!(t.base)
                    .try_into()
                    .map_err(|x: ast::Error| anyhow!("{:?}", x))?;
                self.write_decode_type(w, field, base, ident)?;
                return Ok(());
            }
            ast::Type::EnumType(t) => {
                let base = ptr!(t.base->base_type);
                let tmp = format!("tmp{}", self.get_seq());
                w.writeln(&format!("let mut {} = Default::default();", tmp))?;
                self.write_decode_type(w, field, base, &tmp)?;
                w.writeln(&format!("{} = {}.into();", ident, tmp))?;
                return Ok(());
            }
            ast::Type::ArrayType(t) => {
                if Self::is_primitive_array(&t) {
                    let len = ptr!(t.length_value);
                    w.writeln(&format!("{} = [0; {}];", ident, len))?;
                    w.writeln(&format!("for i in 0..{} {{", len))?;
                    {
                        let _scope = w.enter_indent_scope();
                        self.write_decode_type(
                            w,
                            field,
                            ptr!(t.element_type),
                            &format!("{}[i]", ident),
                        )?;
                    }
                    w.writeln("}")?;
                } else {
                    let len = match ptr_null!(t.length_value) {
                        None => self.expr_to_string(&ptr!(t.length))?,
                        Some(x) => x.to_string(),
                    };
                    w.writeln(&format!("{} = Vec::new();", ident))?;
                    w.writeln(&format!("for _ in 0..{} {{", len))?;
                    {
                        let _scope = w.enter_indent_scope();
                        let temp = format!("tmp{}", self.get_seq());
                        w.writeln(&format!("let mut {} = Default::default();", temp))?;
                        self.write_decode_type(w, field, ptr!(t.element_type), &temp)?;
                        w.writeln(&format!("{}.push({});", ident, temp))?;
                    }
                }
                return Ok(());
            }
            _ => {}
        }
        Err(anyhow!("unsupported decode type"))
    }

    pub fn write_encode_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
    ) -> Result<()> {
        let ident = self.expr_to_string(&ptr!(field.ident).into())?;
        self.write_encode_type(w, &field.borrow(), ptr!(field.field_type), &ident)?;
        Ok(())
    }

    pub fn write_decode_field<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        field: &SharedPtr<ast::Field>,
    ) -> Result<()> {
        let ident = self.expr_to_string(&ptr!(field.ident).into())?;
        self.write_decode_type(w, &field.borrow(), ptr!(field.field_type), &ident)?;
        Ok(())
    }

    pub fn write_format_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        fmt: SharedPtr<ast::Format>,
    ) -> Result<()> {
        let ident = ptr_null!(fmt.ident.ident);
        self.map_file.structs.push(ident.clone());
        w.writeln("#[derive(Default,Debug,Clone,PartialEq)]")?;
        w.writeln(&format!("pub struct {} {{", ident))?;
        {
            let _scope = w.enter_indent_scope();
            self.write_struct_type(w, ptr!(fmt.body.struct_type))?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_if<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        node: SharedPtr<ast::If>,
    ) -> Result<()> {
        let cond = ptr!(node.cond);
        let cond = self
            .s
            .to_string(&cond.into())
            .map_err(|x| anyhow!("{:?}", x))?;
        w.writeln(&format!("if {cond} {{"))?;
        {
            let _scope = w.enter_indent_scope();
            self.write_node(w, ptr!(node.then).into())?;
        }
        w.writeln("}")?;
        if let Some(else_) = ptr_null!(node.els) {
            w.writeln("else ")?;
            if let ast::Node::If(els) = else_.clone().into() {
                self.write_if(w, els)?;
            } else {
                w.writeln("{")?;
                let _scope = w.enter_indent_scope();
                self.write_node(w, else_)?;
                w.writeln("}")?;
            }
        }
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
            ast::Node::ScopedStatement(stmt) => {
                self.write_node(w, ptr!(stmt.statement).into())?;
            }
            ast::Node::If(if_) => {
                self.write_if(w, if_)?;
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
        w.writeln("fn encode<W:std::io::Write>(&self,w :&mut W) -> Result<(),std::io::Error> {")?;
        {
            let _scope = w.enter_indent_scope();
            self.write_node(w, ast::Node::IndentBlock(ptr!(ty.body)))?;
            w.writeln("Ok(())")?;
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
            "fn decode<R:std::io::Read>(r :&mut R) -> Result<{ident},std::io::Error> {{"
        ))?;
        {
            let _scope = w.enter_indent_scope();
            w.writeln("let mut d = Self::default();")?;
            w.writeln("d.decode_impl(r)?;")?;
            w.writeln("Ok(d)")?;
        }
        w.writeln("}")?;
        w.writeln(&format!(
            "fn decode_impl<R :std::io::Read>(&mut self,r :&mut R) -> Result<(),std::io::Error> {{"
        ))?;
        {
            let _scope = w.enter_indent_scope();
            self.write_node(w, ast::Node::IndentBlock(ptr!(ty.body)))?;
            w.writeln("Ok(())")?;
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
        w.writeln(&format!("impl {} {{", ptr_null!(fmt.ident.ident)))?;
        {
            let _scope = w.enter_indent_scope();
            self.encode = true;
            self.write_encode_fn(w, fmt.clone())?;
            self.encode = false;
            self.write_decode_fn(w, fmt.clone())?;
        }
        w.writeln("}")?;
        Ok(())
    }

    pub fn write_enum(&mut self, enum_: SharedPtr<ast::Enum>) -> Result<()> {
        let ident = ptr_null!(enum_.ident.ident);
        self.w.writeln("#[derive(Default,Debug,Clone,PartialEq)]")?;
        self.w.writeln(&format!("pub enum {} {{", ident))?;
        let mut first = true;
        {
            let _scope = self.w.enter_indent_scope();
            for elem in ptr_null!(enum_.members).iter() {
                if first {
                    self.w.writeln("#[default]")?;
                }
                self.w
                    .writeln(&format!("{},", ptr_null!(elem.ident.ident)))?;

                self.s.add_map(
                    &ptr!(elem.ident),
                    &format!("{}::{}", ident, ptr_null!(elem.ident.ident)),
                );
                first = false;
            }
            if let Some(base) = ptr_null!(enum_.base_type) {
                let base = self.get_type(&base)?;
                if first {
                    self.w.writeln("#[default]")?;
                }
                self.w.writeln(&format!("{}Other({}),", ident, base))?;
            }
        }
        self.w.writeln("}")?;
        if let Some(ast::Type::IntType(x)) = ptr_null!(enum_.base_type) {
            let bit_size = ptr!(x.bit_size);
            let base = self.get_int_type(
                bit_size,
                ptr_null!(x.is_signed),
                ptr_null!(x.is_common_supported),
            )?;
            self.w.writeln(&format!("impl {} {{", ident))?;
            {
                let _scope = self.w.enter_indent_scope();
                let mut write_bytes = |le_or_be: &str| -> Result<()> {
                    self.w.writeln(&format!(
                        "pub fn to_{le_or_be}_bytes(&self) -> [u8;{}] {{",
                        bit_size / 8
                    ))?;
                    {
                        let _scope = self.w.enter_indent_scope();
                        let tmp = format!("tmp{}", self.get_seq());
                        self.w
                            .writeln(&format!("let {tmp} :{base} = match self {{"))?;
                        {
                            let _scope = self.w.enter_indent_scope();
                            for elem in ptr_null!(enum_.members).iter() {
                                self.w.writeln(&format!(
                                    "{}::{} => {},",
                                    ident,
                                    ptr_null!(elem.ident.ident),
                                    self.expr_to_string(&ptr!(elem.value))?
                                ))?;
                            }
                        }
                        self.w
                            .writeln(&format!("{}::{}Other(x) => *x,", ident, ident))?;
                        self.w.writeln("};")?;
                        self.w.writeln(&format!("{tmp}.to_{le_or_be}_bytes()"))?;
                    }
                    self.w.writeln("}")?;

                    Ok(())
                };
                write_bytes("le")?;
                write_bytes("be")?;
            }
            self.w.writeln("}")?;

            self.w
                .writeln(&format!("impl From<{base}> for {ident} {{"))?;
            {
                let _scope = self.w.enter_indent_scope();
                self.w.writeln(&format!("fn from(x :{base}) -> Self {{"))?;
                {
                    let _scope = self.w.enter_indent_scope();
                    self.w.writeln("match x {")?;
                    {
                        let _scope = self.w.enter_indent_scope();
                        for elem in ptr_null!(enum_.members).iter() {
                            self.w.writeln(&format!(
                                "{} => {}::{},",
                                self.expr_to_string(&ptr!(elem.value))?,
                                ident,
                                ptr_null!(elem.ident.ident)
                            ))?;
                        }
                    }
                    self.w
                        .writeln(&format!("_ => {}::{}Other(x),", ident, ident))?;
                    self.w.writeln("}")?;
                }
                self.w.writeln("}")?;
            }
            self.w.writeln("}")?;
        }
        Ok(())
    }

    pub fn write_program(&mut self, in_prog: SharedPtr<ast::Program>) -> Result<()> {
        let prog = in_prog.borrow();
        for elem in prog.elements.iter() {
            if let Ok(enum_) = elem.try_into() {
                self.write_enum(enum_)?;
            }
        }
        let sorted = topological_sort_format(&in_prog).ok_or(anyhow!("error"))?;
        let mut w = Writer::new(Vec::new());
        w.prefix = "$SELF".to_string();
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
