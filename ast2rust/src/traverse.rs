use super::ast;
use super::test;
use std::ops::Fn;

pub fn traverse<F>(n: &ast::Node, f: &F)
where
    F: Fn(&ast::Node) -> (),
{
    ast::walk_node(n, &|s: &dyn ast::Visitor, n: &ast::Node| -> bool {
        ast::walk_node(n, s);
        f(n);
        true
    })
}

#[cfg(test)]
mod tests {
    use serde::Deserialize;

    use super::super::ast;
    use super::test;
    use super::traverse;

    #[test]
    fn test_traverse() {
        let ch = test::execAndOutput("./example/feature_test/tree_test.bgn").unwrap();
        let mut de = serde_json::Deserializer::from_slice(&ch.stdout);
        let file = ast::AstFile::deserialize(&mut de).unwrap();
        let prog = ast::parse_ast(file.ast.unwrap()).unwrap();
        let prog = prog.into();
        traverse(&prog, &mut |n: &ast::Node| {
            let t: ast::NodeType = n.into();
            println!("node: {:?}", t);
        });
    }
}
