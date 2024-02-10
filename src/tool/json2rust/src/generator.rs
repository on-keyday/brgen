use ast2rust::ast;
use std::cell::RefCell;
use std::io::{self, Write};
use std::rc::{Rc, Weak};

type SharedPtr<T> = Rc<RefCell<T>>;
type WeakPtr<T> = Weak<RefCell<T>>;

pub struct Generator<W: std::io::Write> {
    writer: std::io::BufWriter<W>,
    indent: usize,
    seq: usize,
    should_indent: bool,
}

impl<W: std::io::Write> Generator<W> {
    pub fn new(w: W) -> Self {
        Self {
            writer: std::io::BufWriter::new(w),
            indent: 0,
            seq: 0,
            should_indent: false,
        }
    }

    fn enter_indent_scope(&mut self) {
        self.indent += 1;
    }

    fn exit_indent_scope(&mut self) {
        self.indent -= 1;
    }

    fn write_indent(&mut self) {
        for _ in 0..self.indent {
            self.writer.write(b"    ").unwrap();
        }
    }

    fn get_seq(&mut self) -> usize {
        let seq = self.seq;
        self.seq += 1;
        seq
    }

    fn write(&mut self, s: &str) {
        self.write_if_should_indent();
        self.writer.write(s.as_bytes()).unwrap();
    }

    fn write_if_should_indent(&mut self) {
        if self.should_indent {
            self.write_indent();
            self.should_indent = false;
        }
    }

    fn writeln(&mut self, s: &str) {
        self.write(s);
        self.write("\n");
        self.should_indent = true;
    }

    pub fn get_type(typ: &ast::Type) -> String {
        match typ {
            ast::Type::IntType(t) => {
                if t.borrow().is_common_supported {
                    if t.borrow().is_signed {
                        format!("i{}", t.borrow().bit_size.clone().unwrap())
                    } else {
                        format!("u{}", t.borrow().bit_size.clone().unwrap())
                    }
                } else {
                    let v = t.borrow().bit_size.clone().unwrap();
                    if v < 64 {
                        let aligned = (v + 7) / 8;
                        if t.borrow().is_signed {
                            format!("i{}", aligned * 8)
                        } else {
                            format!("u{}", aligned * 8)
                        }
                    } else {
                        format!("i{}", v)
                    }
                }
            }
            ast::Type::BoolType(_) => "bool".to_string(),
            ast::Type::EnumType(t) => {
                let enum_ = t.borrow().base.clone().unwrap();
                let enum_ = enum_.upgrade().unwrap();
                let x = enum_.borrow().ident.clone().unwrap().borrow().ident.clone();
                x
            }
            ast::Type::StructType(t) => {
                let struct_ = t.borrow().base.clone().unwrap();
                match struct_ {
                    ast::NodeWeak::Format(struct_) => {
                        let struct_ = struct_.upgrade().unwrap();
                        let x = struct_
                            .borrow()
                            .ident
                            .clone()
                            .unwrap()
                            .borrow()
                            .ident
                            .clone();
                        x
                    }
                    x => {
                        eprintln!("{:?}", ast::NodeType::try_from(x));
                        todo!("unsupported")
                    }
                }
            }
            ast::Type::IdentType(t) => t.borrow().ident.clone().unwrap().borrow().ident.clone(),
            ast::Type::ArrayType(t) => {
                let ty = t.borrow().base_type.clone().unwrap();
                let ty = Self::get_type(&ty);
                if t.borrow().length_value.is_some() {
                    format!("[{}; {}]", ty, t.borrow().length_value.clone().unwrap())
                } else {
                    format!("Vec<{}>", ty)
                }
            }
            x => {
                eprintln!("{:?}", ast::NodeType::from(ast::Node::from(x)));
                todo!("unsupported")
            }
        }
    }

    pub fn write_field(&mut self, field: &SharedPtr<ast::Field>) {
        if field.borrow().ident.is_none() {
            let some = Some(Rc::new(RefCell::new(ast::Ident {
                loc: field.borrow().loc.clone(),
                expr_type: field.borrow().field_type.clone(),
                ident: format!("hidden_field_{}", self.get_seq()),
                base: Some(ast::NodeWeak::Field(Rc::downgrade(&field))),
                constant_level: ast::ConstantLevel::Variable,
                usage: ast::IdentUsage::DefineField,
                scope: None,
            })));
            field.borrow_mut().ident = some;
        }
        let ident = field.borrow().ident.clone().unwrap();
        self.write("pub ");
        self.write(&format!("{}: ", ident.borrow().ident));
        let typ = Self::get_type(&field.borrow().field_type.clone().unwrap());
        self.writeln(&format!("{},", typ));
    }

    pub fn write_struct_type_impl(&mut self, ty: SharedPtr<ast::StructType>) -> io::Result<()> {
        let ty = ty.borrow();
        for field in ty.fields.iter() {
            if let ast::Member::Field(field) = field {
                self.write_field(field);
            }
        }
        Ok(())
    }

    pub fn write_struct_type(&mut self, ty: SharedPtr<ast::StructType>) -> io::Result<()> {
        match ty.borrow().base.clone().unwrap().try_into() {
            Ok(ast::Node::Format(node)) => {
                let ident: SharedPtr<ast::Ident> = node.borrow().ident.clone().unwrap();
                let ident = ident.borrow().ident.clone();
                self.writeln(&format!("pub struct {} {{", ident));
                self.enter_indent_scope();
                self.write_struct_type_impl(ty.clone())?;
                self.exit_indent_scope();
                self.writeln("}");
            }
            _ => {
                self.write_struct_type_impl(ty.clone())?;
            }
        }
        Ok(())
    }

    pub fn write_format(&mut self, fmt: SharedPtr<ast::Format>) -> io::Result<()> {
        let fmt = fmt.borrow();
        let block = fmt.body.clone().unwrap();
        let block = block.borrow();
        let struct_type = block.struct_type.clone().unwrap();
        self.write_struct_type(struct_type)?;
        Ok(())
    }

    pub fn write_program(&mut self, prog: SharedPtr<ast::Program>) -> io::Result<()> {
        let prog = prog.borrow();
        for elem in prog.elements.iter() {
            if let Ok(fmt) = elem.try_into() {
                self.write_format(fmt)?;
            }
        }
        Ok(())
    }
}
