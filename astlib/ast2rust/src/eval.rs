use std::{
    cell::RefCell,
    collections::{HashMap, HashSet},
    rc::Rc,
};

use crate::traverse;

use super::ast;

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

pub type Result = std::result::Result<EvalValue, EvalError>;

impl Evaluator {
    pub fn new() -> Self {
        Self {}
    }

    pub fn eval(&self, expr: &ast::Expr) -> Result {
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

mod tests {
    use super::{super::test, topological_sort_format};
    use serde::Deserialize;

    #[test]
    fn test_sort() {
        let ch = test::exec_and_output("./example/feature_test/sort_test.bgn").unwrap();
        let mut de = serde_json::Deserializer::from_slice(&ch.stdout);
        let file = super::ast::AstFile::deserialize(&mut de).unwrap();
        let prog = super::ast::parse_ast(file.ast.unwrap()).unwrap();
        let sorted = topological_sort_format(&prog).unwrap();
        for fmt in sorted.iter() {
            println!("{:?}", fmt.borrow().ident.clone().unwrap().borrow().ident);
        }
    }
}
