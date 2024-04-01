use std::{cell::RefCell, collections::HashMap, rc::Rc};

use ast2rust::{
    ast::{Enum, EnumMember, Format, Node, Program},
    eval::Evaluator,
};
use inkwell::{context::Context, types::AnyTypeEnum, values::AnyValueEnum};

enum Error {
    UnwrapError,
}

type Result<T> = std::result::Result<T, Error>;

pub struct Generator<'ctx> {
    context: Context,
    eval: Evaluator,
    prog: &'ctx Rc<RefCell<Program>>,
}

struct LocalContext<'ctx> {
    pub enum_map: HashMap<usize, AnyValueEnum<'ctx>>,
}

impl<'ctx> Generator<'ctx> {
    pub fn new(prog: &'ctx Rc<RefCell<Program>>) -> Self {
        Self {
            context: Context::create(),
            eval: Evaluator::new(),
            prog,
        }
    }

    fn generate_format(&self, local: &mut LocalContext, fmt: &Rc<RefCell<Format>>) -> Result<()> {
        let struct_typ = fmt
            .borrow()
            .body
            .clone()
            .ok_or(Error::UnwrapError)?
            .borrow()
            .struct_type
            .clone()
            .unwrap();
        Ok(())
    }

    pub fn generate(&'ctx self) -> Result<()> {
        let r = self.prog.borrow();
        let mut local = LocalContext {
            enum_map: HashMap::new(),
        };
        for f in r.elements.iter() {
            match f {
                Node::Enum(e) => {
                    for c in e.borrow().members.iter() {
                        let expr = c.borrow().value.clone().unwrap();
                        let value = self.eval.eval(&expr).unwrap(); // TODO: handle error
                        match value {
                            ast2rust::eval::EvalValue::Int(i) => {
                                let i = self.context.i64_type().const_int(i as u64, false);
                                let hash_index = c.as_ptr() as usize;
                                local.enum_map.insert(hash_index, i.into());
                            }
                            _ => {}
                        }
                    }
                }
                _ => {}
            }
        }
        for f in r.elements.iter() {
            match f {
                Node::Format(e) => {
                    self.generate_format(&mut local, e)?;
                }
                _ => {}
            }
        }
        Ok(())
    }
}
