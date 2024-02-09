use ast2rust::ast2rust::ast::{self, Expr};
use std::cell::RefCell;
use std::io::Write;
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

    pub fn write_struct_type(&mut self, ty: SharedPtr<ast::StructType>) {
        let ty = ty.borrow();
        if let Ok(node) = ty.base.unwrap().clone().try_into() {}

        for field in ty.fields.iter() {
            if let ast::Member::Field(field) = field {
                self.write_field(field);
            }
        }
    }

    pub fn write_format(&mut self, fmt: SharedPtr<ast::Format>) -> Option<()> {
        let fmt = fmt.borrow();
        let block = fmt.body.clone().unwrap();
        let block = block.borrow();
        let struct_type = block.struct_type.clone().unwrap();
        self.write_struct_type(struct_type);
        Some(())
    }

    pub fn write_program(&mut self, prog: SharedPtr<ast::Program>) {
        let prog = prog.borrow();
        for elem in prog.elements.iter() {
            if let Ok(fmt) = elem.try_into() {
                self.write_format(fmt);
            }
        }
    }
}
