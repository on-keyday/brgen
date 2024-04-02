use std::{cell::RefCell, collections::HashMap, rc::Rc};

use ast2rust::{
    ast::{EnumMember, Field, Format, Member, Node, NodeType, Program, Type},
    eval::{EvalError, Evaluator},
};
use inkwell::{
    context::Context,
    types::{BasicTypeEnum, StructType},
    values::AnyValueEnum,
};

#[derive(Debug)]
pub enum Error {
    JSONError(serde_json::Error),
    ParseError(ast2rust::ast::Error),
    UnwrapError,
    EvalError(EvalError),
    TryFromIntError(std::num::TryFromIntError),
    Unsupported(NodeType),
}

impl From<EvalError> for Error {
    fn from(e: EvalError) -> Self {
        Self::EvalError(e)
    }
}

impl From<std::num::TryFromIntError> for Error {
    fn from(e: std::num::TryFromIntError) -> Self {
        Self::TryFromIntError(e)
    }
}

type Result<T> = std::result::Result<T, Error>;

pub struct Generator<'ctx> {
    context: Context,
    eval: Evaluator,
    prog: &'ctx Rc<RefCell<Program>>,
}

pub struct FieldInfo<'ctx> {
    pub field: Rc<RefCell<Field>>,
    pub llvm_type: BasicTypeEnum<'ctx>,
}

struct LocalContext<'ctx> {
    pub enum_map: HashMap<*mut EnumMember, AnyValueEnum<'ctx>>,
    pub field_map: HashMap<*mut Field, FieldInfo<'ctx>>,
    pub fmt_map: HashMap<*mut Format, StructType<'ctx>>,
}

impl<'ctx> Generator<'ctx> {
    pub fn new(prog: &'ctx Rc<RefCell<Program>>) -> Self {
        Self {
            context: Context::create(),
            eval: Evaluator::new(),
            prog,
        }
    }

    fn type_to_llvm(&'ctx self, t: &Type) -> Result<BasicTypeEnum<'ctx>> {
        match t {
            Type::IntType(i) => {
                let ity = self.context.custom_width_int_type(
                    i.borrow().bit_size.ok_or(Error::UnwrapError)?.try_into()?,
                );
                Ok(BasicTypeEnum::IntType(ity))
            }
            Type::ArrayType(a) => {
                let elem_ty = a.borrow().element_type.clone().ok_or(Error::UnwrapError)?;
                if let Some(_) = a.borrow().length_value {
                    self.type_to_llvm(&elem_ty)
                        .map(|ty| BasicTypeEnum::ArrayType(ty.into_array_type()))
                } else {
                    let elem = self.type_to_llvm(t)?;
                    let ptr = elem.into_pointer_type();
                    let t = self.context.struct_type(
                        &[
                            ptr.into(),
                            // TODO(on-keyday): make this size_t
                            self.context.i64_type().into(),
                        ],
                        false,
                    );
                    Ok(BasicTypeEnum::StructType(t))
                }
            }
            Type::BoolType(_) => Ok(BasicTypeEnum::IntType(self.context.bool_type())),
            _ => Err(Error::Unsupported(t.into())),
        }
    }

    fn generate_format(
        &'ctx self,
        local: &mut LocalContext<'ctx>,
        fmt: &Rc<RefCell<Format>>,
    ) -> Result<()> {
        let struct_typ = fmt
            .borrow()
            .body
            .clone()
            .ok_or(Error::UnwrapError)?
            .borrow()
            .struct_type
            .clone()
            .ok_or(Error::UnwrapError)?;
        let mut fields = Vec::new();
        struct_typ
            .borrow()
            .fields
            .iter()
            .try_for_each(|f| -> Result<()> {
                match f {
                    Member::Field(f) => {
                        let typ = f.borrow().field_type.clone().ok_or(Error::UnwrapError)?;
                        let llvm = self.type_to_llvm(&typ)?;
                        fields.push(llvm.clone());
                        local.field_map.insert(
                            f.as_ptr(),
                            FieldInfo {
                                field: f.clone(),
                                llvm_type: llvm,
                            },
                        );
                    }
                    _ => {}
                }
                Ok(())
            })?;
        let struct_ty = self.context.struct_type(&fields, false);
        local.fmt_map.insert(fmt.as_ptr(), struct_ty);
        Ok(())
    }

    pub fn generate(&'ctx self) -> Result<()> {
        let r = self.prog.borrow();
        let mut local = LocalContext {
            enum_map: HashMap::new(),
            field_map: HashMap::new(),
            fmt_map: HashMap::new(),
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
                                local.enum_map.insert(c.as_ptr(), i.into());
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
