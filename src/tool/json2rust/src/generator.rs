use anyhow::{anyhow, Result};
use ast2rust::ast;
use ast2rust_macro::{ptr, ptr_null};
use std::cell::RefCell;
use std::rc::Rc;

type SharedPtr<T> = Rc<RefCell<T>>;

pub struct Generator<W: std::io::Write> {
    w: Writer<W>,
    seq: usize,
    encode: bool,
}

pub struct Writer<W: std::io::Write> {
    writer: W,
    indent: usize,

    should_indent: bool,
}

impl<W: std::io::Write> Writer<W> {
    pub fn new(w: W) -> Self {
        Self {
            writer: w,
            indent: 0,
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

    fn enter_indent_scope(&mut self) {
        self.indent += 1;
    }

    fn exit_indent_scope(&mut self) {
        self.indent -= 1;
    }

    fn write_indent(&mut self) -> Result<()> {
        for _ in 0..self.indent {
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
        }
    }

    fn self_w<'a>(&mut self) -> &'a mut Writer<W> {
        unsafe {
            let p = &mut self.w as *const Writer<W>;
            &mut *(p as *mut Writer<W>)
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

    pub fn get_type(typ: &ast::Type) -> Result<String> {
        match typ {
            ast::Type::IntType(t) => {
                if t.borrow().is_common_supported {
                    if t.borrow().is_signed {
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
                let ty = Self::get_type(&ty)?;
                if t.borrow().length_value.is_some() {
                    Ok(format!("[{}; {}]", ty, ptr!(t.length_value)))
                } else {
                    Ok(format!("Vec<{}>", ty))
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
        let typ = Self::get_type(&ptr!(field.field_type))?;
        w.writeln(&format!("{},", typ))?;
        Ok(())
    }

    pub fn write_struct_type_impl<W1: std::io::Write>(
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
        let ty = Self::get_type(&ptr!(field.field_type))?;
        w.writeln(&format!(
            "let {} = map.next_value::<{}>(\"{}\");",
            ident, ty, ident
        ))?;
        Ok(())
    }

    pub fn write_struct_type<W1: std::io::Write>(
        &mut self,
        w: &mut Writer<W1>,
        ty: SharedPtr<ast::StructType>,
    ) -> Result<()> {
        match ptr!(ty.base).try_into() {
            Ok(ast::Node::Format(node)) => {
                let ident = ptr_null!(node.ident.ident);
                w.writeln(&format!("pub struct {} {{", ident))?;
                w.enter_indent_scope();
                self.write_struct_type_impl(w, ty.clone())?;
                w.exit_indent_scope();
                w.writeln("}")?;
            }
            _ => {
                self.write_struct_type_impl(w, ty.clone())?;
            }
        }
        Ok(())
    }

    pub fn write_node(&mut self, node: ast::Node) -> Result<()> {
        match node {
            ast::Node::IndentBlock(block) => {
                for elem in ptr_null!(block.elements).iter() {
                    self.write_node(elem.clone())?;
                }
            }
            ast::Node::Format(fmt) => {
                self.write_format(fmt)?;
            }
            ast::Node::Field(field) => {
                let w = self.self_w();
                if self.encode {
                    self.write_encode_field(w, &field)?;
                } else {
                    self.write_decode_field(w, &field)?;
                }
            }
            _ => {
                eprintln!("{:?}", ast::NodeType::try_from(node));
                todo!("unsupported")
            }
        }
        Ok(())
    }

    pub fn write_format(&mut self, fmt: SharedPtr<ast::Format>) -> Result<()> {
        let struct_type = ptr!(fmt.body.struct_type);
        //SAFETY: We are casting a mutable reference to a mutable reference, which is safe.
        let w = self.self_w();
        self.write_struct_type(w, struct_type)?;
        self.encode = true;
        self.write_node(ast::Node::IndentBlock(ptr!(fmt.body)))?;
        Ok(())
    }

    pub fn write_program(&mut self, prog: SharedPtr<ast::Program>) -> Result<()> {
        let prog = prog.borrow();
        for elem in prog.elements.iter() {
            if let Ok(fmt) = elem.try_into() {
                self.write_format(fmt)?;
            }
        }
        self.w.flush()?;
        Ok(())
    }
}
