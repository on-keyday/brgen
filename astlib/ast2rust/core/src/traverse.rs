use super::ast;
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

    use super::super::ast;
    use super::super::test;
    use super::traverse;

    #[test]
    fn test_traverse() {
        let prog = test::load_test_file();
        traverse(&prog, &mut |n: &ast::Node| {
            let t: ast::NodeType = n.into();
            println!("node: {:?}", t);
        });
    }
}
