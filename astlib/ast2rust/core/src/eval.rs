use std::{
    cell::{Ref, RefCell},
    collections::{HashMap, HashSet},
    fmt::Write,
    rc::Rc,
};

use crate::{
    ast::{Binary, MemberWeak},
    traverse, PtrKey, PtrUnwrapError,
};

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
    pub self_: String,
    ident_map: HashMap<PtrKey<ast::Ident>, String>,
    pub error_string: String,
    pub mutator: String,
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

pub fn is_any_range(r: &ast::Expr) -> bool {
    match r {
        ast::Expr::Range(b) => {
            if let ast::BinaryOp::RangeExclusive | ast::BinaryOp::RangeInclusive = ptr_null!(b.op) {
                return ptr_null!(b.start).is_none() && ptr_null!(b.end).is_none();
            }
        }
        ast::Expr::Identity(i) => {
            if let Some(e) = &ptr_null!(i.expr) {
                return is_any_range(e);
            }
        }
        ast::Expr::Paren(p) => {
            if let Some(e) = &ptr_null!(p.expr) {
                return is_any_range(e);
            }
        }
        _ => {}
    }
    false
}

pub fn is_struct_type(r: &ast::Type) -> bool {
    match r {
        ast::Type::StructType(_) => true,
        ast::Type::IdentType(i) => {
            if let Some(b) = i.borrow().base.as_ref() {
                if let Some(i) = b.upgrade() {
                    return is_struct_type(&i);
                }
            }
            false
        }
        _ => false,
    }
}

pub fn is_primitive_array(t: &Rc<RefCell<ast::ArrayType>>) -> bool {
    match t.borrow().element_type.as_ref() {
        Some(ast::Type::IntType(_)) => ptr_null!(t.length_value).is_some(),
        _ => false,
    }
}

pub fn is_non_primitive_array(t: &ast::Type) -> bool {
    match t {
        ast::Type::ArrayType(t) => !is_primitive_array(t),
        _ => false,
    }
}

pub fn get_int_type(
    bit_size: u64,
    is_signed: bool,
    is_common_supported: bool,
) -> Result<String, StringerError> {
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

pub fn get_type_prim(typ: &ast::Type) -> Result<String, StringerError> {
    match typ {
        ast::Type::IntType(t) => {
            let x = get_int_type(
                ptr!(t.bit_size),
                ptr_null!(t.is_signed),
                ptr_null!(t.is_common_supported),
            )?;
            Ok(x)
        }
        ast::Type::FloatType(t) => {
            let x = ptr!(t.bit_size);
            Ok(format!("f{}", x))
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
            let ty = get_type_prim(&ty)?;
            if is_primitive_array(t) {
                Ok(format!("[{}; {}]", ty, ptr!(t.length_value)))
            } else {
                Ok(format!("Vec<{}>", ty))
            }
        }
        _ => Err(StringerError::Unsupported),
    }
}

impl Stringer {
    pub fn new(self_: String) -> Self {
        Self {
            self_,
            ident_map: HashMap::new(),
            error_string: "".to_string(),
            mutator: "".to_string(),
        }
    }

    pub fn add_map(&mut self, ident: &Rc<RefCell<ast::Ident>>, s: &str) {
        let key = PtrKey::new(&ident);
        self.ident_map.insert(key, s.to_string());
    }

    pub fn replace_with_this(&self, this: &str, s: &str) -> Result<String, StringerError> {
        if let Some(_) = s.find("$SELF") {
            Ok(s.replace("$SELF", this)
                .replace("$ERROR", &self.error_string)
                .replace("$MUT", &self.mutator))
        } else {
            Ok(s.to_string()
                .replace("$ERROR", &self.error_string)
                .replace("$MUT", &self.mutator))
        }
    }

    pub fn to_map_ident(
        &self,
        ident: &Rc<RefCell<ast::Ident>>,
        default_: &str,
    ) -> Result<String, StringerError> {
        let ident_str = &ident.borrow().ident;
        let (ident, via_member_access) = if let Some(x) = lookup_base(ident) {
            x
        } else {
            return Ok(ident_str.clone());
        };
        let ident_str = &ident.borrow().ident;
        let key = PtrKey::new(&ident);
        let ident_str = self.ident_map.get(&key).or(Some(&ident_str)).unwrap();
        if !via_member_access {
            if let Some(_) = ident
                .borrow()
                .base
                .as_ref()
                .and_then(|s| s.upgrade())
                .and_then(|s| s.try_into_member().ok())
            {
                return self.replace_with_this(&self.self_, ident_str);
            }
        }
        if let Some(x) = ident
            .borrow()
            .base
            .as_ref()
            .and_then(|f| f.upgrade())
            .and_then(|s| s.try_into_member().ok())
            .and_then(|s| Some(matches!(s, ast::Member::EnumMember(_))))
        {
            if x {
                return Ok(ident_str.clone());
            }
        }
        self.replace_with_this(&default_, ident_str)
    }

    pub fn eval_binary_op(
        &self,
        b: &Rc<RefCell<Binary>>,
        root: bool,
    ) -> Result<String, StringerError> {
        let left = self.to_string(&ptr!(b.left))?;
        let right = self.to_string(&ptr!(b.right))?;
        if root {
            Ok(format!("{} {} {}", left, ptr_null!(b.op).to_str(), right))
        } else {
            Ok(format!("({} {} {})", left, ptr_null!(b.op).to_str(), right))
        }
    }

    pub fn eval_ephemeral_binary_op(
        &self,
        op: ast::BinaryOp,
        left: &ast::Expr,
        right: &ast::Expr,
    ) -> Result<String, StringerError> {
        let tmp = ast::Binary {
            loc: ast::Loc {
                pos: ast::Pos { begin: 0, end: 0 },
                line: 0,
                col: 0,
                file: 0,
            },
            expr_type: None,
            left: Some(left.clone()),
            right: Some(right.clone()),
            op,
            constant_level: ast::ConstantLevel::Variable,
        };
        let b = Rc::new(RefCell::new(tmp));
        self.eval_binary_op(&b, true)
    }

    pub fn to_string(&self, n: &ast::Expr) -> Result<String, StringerError> {
        self.to_string_impl(n, true)
    }

    pub fn to_string_impl(&self, n: &ast::Expr, root: bool) -> Result<String, StringerError> {
        match n {
            ast::Expr::IntLiteral(i) => Ok(ptr_null!(i.value)),
            ast::Expr::BoolLiteral(b) => Ok(ptr_null!(b.value).to_string()),
            ast::Expr::StrLiteral(s) => Ok(ptr_null!(s.value)),
            ast::Expr::Ident(i) => self.to_map_ident(&i, ""),
            ast::Expr::Binary(b) => {
                return self.eval_binary_op(b, root);
            }
            ast::Expr::Unary(u) => {
                let right = self.to_string_impl(&ptr!(u.expr), false)?;
                Ok(format!("({} {})", ptr_null!(u.op).to_str(), right))
            }
            ast::Expr::Cond(c) => {
                let cond = self.to_string_impl(&ptr!(c.cond), false)?;
                let then = self.to_string_impl(&ptr!(c.then), false)?;
                let else_ = self.to_string_impl(&ptr!(c.els), false)?;
                let common_type_str = ptr!(c.expr_type);
                let common_type_str = get_type_prim(&common_type_str)?;
                Ok(format!(
                    "(if {cond} {{ {then} as {common_type_str} }} else {{ {else_} as {common_type_str} }} )"
                ))
            }
            ast::Expr::MemberAccess(c) => {
                let base = self.to_string_impl(&ptr!(c.target), false)?;
                self.to_map_ident(&ptr!(c.member), &base)
            }
            ast::Expr::Identity(i) => self.to_string_impl(&ptr!(i.expr), root),
            ast::Expr::Available(a) => {
                let target = ptr!(a.target);
                let ty = if let ast::Type::UnionType(t) = match target.get_expr_type() {
                    Some(t) => t,
                    None => return Ok("false".to_string()),
                } {
                    t
                } else {
                    return Ok("false".to_string());
                };
                let cond0 = ptr_null!(ty.base_type->cond).clone();
                let mut w = String::new();
                let mut need_else = false;
                for c in ptr_null!(ty.candidates) {
                    let cond = ptr!(c.cond).upgrade().ok_or_else(|| {
                        StringerError::Unwrap(PtrUnwrapError::ExpiredWeakPtr("cond"))
                    })?;
                    if need_else {
                        w += "else ";
                    }
                    let cond = if is_any_range(&cond) {
                        " {{".to_string()
                    } else if let Some(cond0) = &cond0 {
                        format!(
                            "if {} {{",
                            self.eval_ephemeral_binary_op(
                                ast::BinaryOp::Equal,
                                &cond0.into(),
                                &cond.into()
                            )?
                        )
                    } else {
                        format!("if {} {{", self.to_string(&cond.into())?)
                    };
                    if let Some(_) = ptr_null!(c.field) {
                        write!(&mut w, "{} true }}", cond).unwrap();
                    } else {
                        write!(&mut w, "{} false }}", cond).unwrap();
                    }
                    need_else = true;
                }
                if !ptr_null!(ty.base_type->exhaustive) {
                    if need_else {
                        w += "else ";
                    }
                    w += " { false }";
                }
                Ok(w)
            }
            _ => Ok("".to_string()),
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
