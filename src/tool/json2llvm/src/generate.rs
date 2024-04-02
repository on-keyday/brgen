use std::{cell::RefCell, collections::HashMap, rc::Rc};

use ast2rust::{
    ast::{Enum, EnumMember, Format, Member, Node, NodeType, Program, Type},
    eval::{EvalError, Evaluator},
};
use inkwell::{context::Context, types::AnyTypeEnum, values::AnyValueEnum};

#[derive(Debug)]
enum Error {
    UnwrapError,
    EvalError(EvalError),
    Unsupported(NodeType),
}

impl From<EvalError> for Error {
    fn from(e: EvalError) -> Self {
        Self::EvalError(e)
    }
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

    fn type_to_llvm(&self, t: &Type) -> Result<AnyTypeEnum<'ctx>> {
        match t {
            _ => Err(Error::Unsupported((t.into()))),
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
            .ok_or(Error::UnwrapError)?;
        struct_typ
            .borrow()
            .fields
            .iter()
            .try_for_each(|f| -> Result<()> {
                match f {
                    Member::Field(f) => {
                        let typ = f.borrow().field_type.clone().ok_or(Error::UnwrapError)?;
                        let llvm = self.type_to_llvm(&typ)?;
                    }
                    _ => {}
                }
                Ok(())
            })?;
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
                        let expr = c.borrow().value.clone().ok_or(Error::UnwrapError)?;
                        let value = self.eval.eval(&expr)?; // TODO: handle error
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
