use serde_derive::{Deserialize, Serialize};

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
    pub node: std::vec::Vec<JSONNode>,
    pub scope: std::vec::Vec<JSONScope>,
}

struct AstConstructor {
    pub node: std::vec::Vec<Node>,
    pub scope: std::vec::Vec<Scope>,
}

pub struct Ast {
    pub program: std::rc::Rc<Node>,
}

pub fn parse_ast<'de, R: std::io::Read>(r: R) -> Result<Ast, Error> {
    let res: JSONAst = serde_json::de::from_reader(r)?;
    let mut ast = AstConstructor {
        node: Vec::new(),
        scope: Vec::new(),
    };
    for n in res.node {
        let node = match n.node_type.as_str() {
            _ => return Err(Error::UnexpectedNodeType(n.node_type)),
        };
        ast.node.push(node);
    }
    Err(Error::Unimplemented)
}
