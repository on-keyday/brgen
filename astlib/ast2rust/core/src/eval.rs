use std::{
    cell::{Ref, RefCell},
    collections::{HashMap, HashSet},
    rc::Rc,
};

use crate::{ast::MemberWeak, traverse, PtrKey, PtrUnwrapError};

use super::ast;
use ast2rust_macro::{ptr, ptr_null};

pub struct Evaluator {}

#[derive(Debug)]
pub enum EvalError {
    Unsupported(ast::NodeType),
    Unknown(String),
    ParseIntError(std::num::ParseIntError),
}

impl From<std::num::ParseIntError> for EvalError {
    fn from(e: std::num::ParseIntError) -> Self {
        Self::ParseIntError(e)
    }
}

#[derive(Debug)]
pub enum EvalValue {
    Int(i64),
    Float(f64),
    Bool(bool),
    String(String),
}

impl Evaluator {
    pub fn new() -> Self {
        Self {}
    }

    pub fn eval(&self, expr: &ast::Expr) -> Result<EvalValue, EvalError> {
        match expr {
            ast::Expr::IntLiteral(i) => Ok(EvalValue::Int(i.borrow().value.parse()?)),
            ast::Expr::BoolLiteral(f) => Ok(EvalValue::Bool(f.borrow().value)),
            ast::Expr::StrLiteral(s) => Ok(EvalValue::String(s.borrow().value.clone())),
            _ => {
                let node: ast::Node = expr.into();
                Err(EvalError::Unsupported(node.into()))
            }
        }
    }
}

fn sort_visit(
    x: &Rc<RefCell<ast::Format>>,
    sorted: &mut Vec<Rc<RefCell<ast::Format>>>,
    visited: &mut HashSet<*mut ast::Format>,
    deps: &HashMap<*mut ast::Format, Vec<Rc<RefCell<ast::Format>>>>,
) {
    if visited.contains(&x.as_ptr()) {
        return;
    }
    visited.insert(x.as_ptr());
    if let Some(deps2) = deps.get(&x.as_ptr()) {
        for y in deps2.iter() {
            sort_visit(y, sorted, visited, deps);
        }
    }
    sorted.push(x.clone());
}

pub fn topological_sort_format(
    prog: &Rc<RefCell<ast::Program>>,
) -> Option<Vec<Rc<RefCell<ast::Format>>>> {
    let fmts = RefCell::new(Vec::new());

    traverse::traverse(&prog.into(), &|node| {
        if let ast::Node::Format(fmt) = node {
            let mut fmts = fmts.borrow_mut();
            fmts.push(fmt.clone());
        }
    });

    let mut deps = HashMap::new();

    for fmt in fmts.borrow().iter() {
        for t in fmt.borrow().depends.iter() {
            let typ = t.upgrade()?;
            let typ: ast::Type = typ.borrow().base.clone()?.try_into().ok()?;
            let typ = match typ {
                ast::Type::StructType(st) => st,
                _ => continue,
            };
            let typ: ast::Node = typ.borrow().base.clone()?.try_into().ok()?;
            let typ = match typ {
                ast::Node::Format(fmt) => fmt,
                _ => continue,
            };
            deps.entry(fmt.as_ptr())
                .or_insert_with(|| Vec::new())
                .push(typ.clone());
        }
    }
    fmts.borrow_mut().sort_by(|a, b| {
        let a_len = if let Some(deps) = deps.get(&a.as_ptr()) {
            deps.len()
        } else {
            0
        };
        let b_len = if let Some(deps) = deps.get(&b.as_ptr()) {
            deps.len()
        } else {
            0
        };
        a_len.cmp(&b_len)
    });
    let mut sorted = Vec::new();
    let mut visited = HashSet::new();
    for fmt in fmts.borrow().iter() {
        sort_visit(fmt, &mut sorted, &mut visited, &deps);
    }
    Some(sorted)
}

pub struct Stringer {
    self_: String,
    ident_map: HashMap<PtrKey<ast::Ident>, String>,
}

#[derive(Debug)]
pub enum StringerError {
    Unwrap(PtrUnwrapError),
    IdentLookup(String),
    Unsupported,
}

impl From<PtrUnwrapError> for StringerError {
    fn from(e: PtrUnwrapError) -> Self {
        Self::Unwrap(e)
    }
}

impl std::fmt::Display for StringerError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            Self::Unwrap(e) => write!(f, "{}", e),
            Self::IdentLookup(s) => write!(f, "ident lookup: {}", s),
            Self::Unsupported => write!(f, "unsupported"),
        }
    }
}

pub fn lookup_base(ident: &Rc<RefCell<ast::Ident>>) -> Option<(Rc<RefCell<ast::Ident>>, bool)> {
    let mut ident = ident.clone();
    let mut via_member_access = false;
    loop {
        let base = ptr_null!(ident.base);
        if let Some(base) = base {
            match base {
                ast::NodeWeak::Ident(i) => {
                    let i = i.upgrade()?;
                    ident = i;
                }
                ast::NodeWeak::MemberAccess(m) => {
                    let m = m.upgrade()?;
                    ident = ptr_null!(m.base)?.upgrade()?;
                    via_member_access = true;
                }
                _ => {
                    let _: ast::Node = base.try_into().ok()?; // check not expired
                    return Some((ident, via_member_access));
                }
            }
        } else {
            return Some((ident, via_member_access));
        }
    }
}

impl Stringer {
    pub fn new(self_: String) -> Self {
        Self {
            self_,
            ident_map: HashMap::new(),
        }
    }

    pub fn to_map_ident(
        &self,
        ident: &Rc<RefCell<ast::Ident>>,
        default_: &str,
    ) -> Result<String, StringerError> {
        let ident_str = &ident.borrow().ident;
        let (ident, via_member_access) =
            lookup_base(ident).ok_or_else(|| StringerError::IdentLookup(ident_str.clone()))?;
        let ident_str = &ident.borrow().ident;
        let key = PtrKey::new(&ident);
        let replace_with_this = |this: &str, s: &str| {
            if let Some(_) = s.find("$SELF") {
                Ok(s.replace("$SELF", this))
            } else {
                Ok(s.to_string())
            }
        };
        if !via_member_access {
            if let Some(w) = ident
                .borrow()
                .base
                .as_ref()
                .and_then(|s| s.upgrade())
                .and_then(|s| s.try_into_member().ok())
            {
                return replace_with_this(&self.self_, ident_str);
            }
        }
        if let Some(s) = self.ident_map.get(&key) {
            replace_with_this(&self.self_, s)
        } else {
            replace_with_this(&self.self_, &default_)
        }
    }

    pub fn to_string(&self, n: &ast::Expr) -> Result<String, StringerError> {
        match n {
            ast::Expr::IntLiteral(i) => Ok(ptr_null!(i.value)),
            ast::Expr::BoolLiteral(b) => Ok(ptr_null!(b.value).to_string()),
            ast::Expr::StrLiteral(s) => Ok(ptr_null!(s.value)),
            ast::Expr::Ident(i) => self.to_map_ident(&i, ""),
            ast::Expr::Binary(b) => {
                let left = self.to_string(&ptr!(b.left))?;
                let right = self.to_string(&ptr!(b.right))?;
                Ok(format!("({} {} {})", left, ptr_null!(b.op).to_str(), right))
            }
            ast::Expr::Unary(u) => {
                let right = self.to_string(&ptr!(u.expr))?;
                Ok(format!("({} {})", ptr_null!(u.op).to_str(), right))
            }
            ast::Expr::Cond(c) => {
                let cond = self.to_string(&ptr!(c.cond))?;
                let then = self.to_string(&ptr!(c.then))?;
                let else_ = self.to_string(&ptr!(c.els))?;
                Ok(format!("(if {cond} {{ {then} }} else {{ {else_} }} )"))
            }
            ast::Expr::MemberAccess(c) => {
                let base = self.to_string(&ptr!(c.target))?;
                self.to_map_ident(&ptr!(c.member), &base)
            }
            _ => Err(StringerError::Unsupported),
        }
    }
}

mod tests {
    use super::{super::test, topological_sort_format};
    use serde::Deserialize;

    #[test]
    fn test_sort() {
        let ch = crate::test::exec_and_output("./example/feature_test/sort_test.bgn").unwrap();
        let mut de = serde_json::Deserializer::from_slice(&ch.stdout);
        let file = super::ast::AstFile::deserialize(&mut de).unwrap();
        let prog = super::ast::parse_ast(file.ast.unwrap()).unwrap();
        let sorted = super::topological_sort_format(&prog).unwrap();
        for fmt in sorted.iter() {
            println!("{:?}", fmt.borrow().ident.clone().unwrap().borrow().ident);
        }
    }
}
