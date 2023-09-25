use serde_derive::{Deserialize, Serialize};
use std::cell::RefCell;
use std::rc::{Rc, Weak};

pub use super::ast2::*;

pub enum Error {
    Unimplemented,
    UnexpectedNodeType(String),
    UnexpectedEof,
    JSONError(serde_json::Error),
}

impl From<serde_json::Error> for Error {
    fn from(e: serde_json::Error) -> Self {
        Error::JSONError(e)
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct JSONNode {
    pub node_type: String,
    pub loc: Loc,
    pub body: serde_json::Value,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct JSONScope {
    pub prev: usize,
    pub next: usize,
    pub branch: usize,
    pub objects: std::vec::Vec<usize>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
struct JSONAst {
    pub node: Vec<JSONNode>,
    pub scope: Vec<JSONScope>,
}

struct AstConstructor {
    pub node: Vec<Rc<RefCell<Node>>>,
    pub scope: Vec<Rc<RefCell<Scope>>>,
}

pub struct Ast {
    pub program: Rc<Node>,
}

/*
{"node": [{"node_type": "node","one_of": ["program","expr","binary","unary","cond","ident","call","if","member_access","paren","index","match","range","tmp_var","block_expr","import","literal","int_literal","bool_literal","str_literal","input","output","config","stmt","loop","indent_scope","match_branch","return","break","continue","assert","implicit_yield","member","field","format","function","type","int_type","ident_type","int_literal_type","str_literal_type","void_type","bool_type","array_type","function_type","struct_type","union_type"]},{"node_type": "program","loc": "loc","struct_type": "shared_ptr<struct_type>","elements": "array<shared_ptr<node>>","global_scope": "shared_ptr<scope>"},{"node_type": "expr","one_of": ["binary","unary","cond","ident","call","if","member_access","paren","index","match","range","tmp_var","block_expr","import","literal","int_literal","bool_literal","str_literal","input","output","config"]},{"node_type": "binary","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","op": ["*","/","%","<<",">>","&","+","-","|","^","==","!=","<","<=",">",">=","&&","||","if","else","..","..=","=",":=","::=","+=","-=","*=","/=","%=","<<=",">>=","&=","|=","^=",","],"left": "shared_ptr<expr>","right": "shared_ptr<expr>"},{"node_type": "unary","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","op": ["!","-"],"expr": "shared_ptr<expr>"},{"node_type": "cond","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","cond": "shared_ptr<expr>","then": "shared_ptr<expr>","els_loc": "loc","els": "shared_ptr<expr>"},{"node_type": "ident","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","ident": "string","usage": ["unknown","reference","define_variable","define_const","define_field","define_format","define_fn","reference_type"],"base": "weak_ptr<node>","scope": "shared_ptr<scope>"},{"node_type": "call","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","callee": "shared_ptr<expr>","raw_arguments": "shared_ptr<expr>","arguments": "array<shared_ptr<expr>>","end_loc": "loc"},{"node_type": "if","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","cond": "shared_ptr<expr>","then": "shared_ptr<indent_scope>","els": "shared_ptr<node>"},{"node_type": "member_access","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","target": "shared_ptr<expr>","member": "string","member_loc": "loc"},{"node_type": "paren","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","expr": "shared_ptr<expr>","end_loc": "loc"},{"node_type": "index","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","expr": "shared_ptr<expr>","index": "shared_ptr<expr>","end_loc": "loc"},{"node_type": "match","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","cond": "shared_ptr<expr>","branch": "array<shared_ptr<match_branch>>","scope": "shared_ptr<scope>"},{"node_type": "range","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","op": ["*","/","%","<<",">>","&","+","-","|","^","==","!=","<","<=",">",">=","&&","||","if","else","..","..=","=",":=","::=","+=","-=","*=","/=","%=","<<=",">>=","&=","|=","^=",","],"start": "shared_ptr<expr>","end": "shared_ptr<expr>"},{"node_type": "tmp_var","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","tmp_var": "uint"},{"node_type": "block_expr","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","calls": "array<shared_ptr<node>>","expr": "shared_ptr<expr>"},{"node_type": "import","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","path": "string","base": "shared_ptr<call>","import_desc": "shared_ptr<program>"},{"node_type": "literal","one_of": ["int_literal","bool_literal","str_literal","input","output","config"]},{"node_type": "int_literal","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>","value": "string"},{"node_type": "bool_literal","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>","value": "bool"},{"node_type": "str_literal","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>","value": "string"},{"node_type": "input","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>"},{"node_type": "output","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>"},{"node_type": "config","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>"},{"node_type": "stmt","one_of": ["loop","indent_scope","match_branch","return","break","continue","assert","implicit_yield","member","field","format","function"]},{"node_type": "loop","base_node_type": ["stmt"],"loc": "loc","init": "shared_ptr<expr>","cond": "shared_ptr<expr>","step": "shared_ptr<expr>","body": "shared_ptr<indent_scope>"},{"node_type": "indent_scope","base_node_type": ["stmt"],"loc": "loc","elements": "array<shared_ptr<node>>","scope": "shared_ptr<scope>"},{"node_type": "match_branch","base_node_type": ["stmt"],"loc": "loc","cond": "shared_ptr<expr>","sym_loc": "loc","then": "shared_ptr<node>"},{"node_type": "return","base_node_type": ["stmt"],"loc": "loc","expr": "shared_ptr<expr>"},{"node_type": "break","base_node_type": ["stmt"],"loc": "loc"},{"node_type": "continue","base_node_type": ["stmt"],"loc": "loc"},{"node_type": "assert","base_node_type": ["stmt"],"loc": "loc","cond": "shared_ptr<binary>"},{"node_type": "implicit_yield","base_node_type": ["stmt"],"loc": "loc","expr": "shared_ptr<expr>"},{"node_type": "member","one_of": ["field","format","function"]},{"node_type": "field","base_node_type": ["member","stmt"],"loc": "loc","ident": "shared_ptr<ident>","colon_loc": "loc","field_type": "shared_ptr<type>","raw_arguments": "shared_ptr<expr>","arguments": "array<shared_ptr<expr>>","belong": "weak_ptr<format>"},{"node_type": "format","base_node_type": ["member","stmt"],"loc": "loc","is_enum": "bool","ident": "shared_ptr<ident>","body": "shared_ptr<indent_scope>","belong": "weak_ptr<format>","struct_type": "shared_ptr<struct_type>"},{"node_type": "function","base_node_type": ["member","stmt"],"loc": "loc","ident": "shared_ptr<ident>","parameters": "array<shared_ptr<field>>","return_type": "shared_ptr<type>","belong": "weak_ptr<format>","body": "shared_ptr<indent_scope>","func_type": "shared_ptr<function_type>"},{"node_type": "type","one_of": ["int_type","ident_type","int_literal_type","str_literal_type","void_type","bool_type","array_type","function_type","struct_type","union_type"]},{"node_type": "int_type","base_node_type": ["type"],"loc": "loc","raw": "string","bit_size": "uint"},{"node_type": "ident_type","base_node_type": ["type"],"loc": "loc","ident": "shared_ptr<ident>","base": "weak_ptr<format>"},{"node_type": "int_literal_type","base_node_type": ["type"],"loc": "loc","base": "weak_ptr<int_literal>"},{"node_type": "str_literal_type","base_node_type": ["type"],"loc": "loc","base": "weak_ptr<str_literal>"},{"node_type": "void_type","base_node_type": ["type"],"loc": "loc"},{"node_type": "bool_type","base_node_type": ["type"],"loc": "loc"},{"node_type": "array_type","base_node_type": ["type"],"loc": "loc","end_loc": "loc","base_type": "shared_ptr<type>","length": "shared_ptr<expr>"},{"node_type": "function_type","base_node_type": ["type"],"loc": "loc","return_type": "shared_ptr<type>","parameters": "array<shared_ptr<type>>"},{"node_type": "struct_type","base_node_type": ["type"],"loc": "loc","fields": "array<shared_ptr<member>>"},{"node_type": "union_type","base_node_type": ["type"],"loc": "loc","fields": "array<shared_ptr<struct_type>>"}],"scope": {"prev": "weak_ptr<scope>","next": "shared_ptr<scope>","branch": "shared_ptr<scope>","ident": "array<std::weak_ptr<node>>"},"loc": {"pos": {"begin": "uint","end": "uint"},"file": "uint"}}
*/
fn mapStrToNodeType(s: &str) -> Result<Node, Error> {
    return match s {
        _ => Err(Error::UnexpectedNodeType(s.to_string())),
    };
}

pub fn parse_ast<'de, R: std::io::Read>(r: R) -> Result<Ast, Error> {
    let res: JSONAst = serde_json::de::from_reader(r)?;
    let mut ast = AstConstructor {
        node: Vec::new(),
        scope: Vec::new(),
    };
    for n in res.node {
        let node = mapStrToNodeType(&n.node_type)?;
        ast.node
            .push(std::rc::Rc::new(std::cell::RefCell::new(node)));
    }
    for s in res.scope {
        let scope = Scope {
            prev: None,
            next: None,
            branch: None,
            ident: Vec::new(),
        };
        ast.scope
            .push(std::rc::Rc::new(std::cell::RefCell::new(scope)));
    }
    Err(Error::Unimplemented)
}
