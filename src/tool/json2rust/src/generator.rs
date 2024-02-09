use ast2rust::ast::{self, Expr};
use std::cell::RefCell;
use std::io::{self, Write};
use std::rc::{Rc, Weak};

type SharedPtr<T> = Rc<RefCell<T>>;
type WeakPtr<T> = Weak<RefCell<T>>;

pub struct Generator<W: std::io::Write> {
    writer: std::io::BufWriter<W>,
    indent: usize,
}

struct IndentScope<'a, W: std::io::Write> {
    generator: &'a mut Generator<W>,
    indent: usize,
}

impl<'a, W: std::io::Write> Drop for IndentScope<'a, W> {
    fn drop(&mut self) {
        self.generator.indent = self.indent;
    }
}

impl<W: std::io::Write> Generator<W> {
    pub fn new(w: W) -> Self {
        Self {
            writer: std::io::BufWriter::new(w),
            indent: 0,
        }
    }

    fn new_indent_scope(&mut self) -> IndentScope<W> {
        let indent = self.indent;
        self.indent += 1;
        IndentScope {
            generator: self,
            indent,
        }
    }

    fn write_indent(&mut self) {
        for _ in 0..self.indent {
            self.writer.write(b"    ").unwrap();
        }
    }

    fn write(&mut self, s: &str) {
        self.writer.write(s.as_bytes()).unwrap();
    }

    pub fn write_field(&mut self, field: &SharedPtr<ast::Field>) {}

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
                self.write(&format!("pub struct {} {{\n", ident));
                let _ = self.new_indent_scope();
                self.write_struct_type_impl(ty.clone())?;
                self.write("}\n");
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
