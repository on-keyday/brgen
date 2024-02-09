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
    use serde::Deserialize;

    use super::super::ast;
    use super::traverse;
    use std::env;
    use std::path;
    use std::process::Command;

    #[test]
    fn test_traverse() {
        env::set_current_dir(path::Path::new("..")).unwrap();
        println!("current dir: {:?}", env::current_dir().unwrap());
        let cmd = env::current_dir()
            .unwrap()
            .join(path::Path::new("tool/src2json"));
        #[cfg(target_os = "windows")]
        let cmd = cmd.with_extension("exe");
        println!("cmd: {:?}", cmd);
        let mut cmd = Command::new(cmd);
        let ch = cmd.arg("./example/tree_test.bgn").output();
        if let Err(e) = ch {
            println!("spawn error: {:?}", e);
            return;
        }
        let ch = ch.unwrap();
        let mut de = serde_json::Deserializer::from_slice(&ch.stdout);
        let file = ast::AstFile::deserialize(&mut de).unwrap();
        let prog = ast::parse_ast(file.ast.unwrap()).unwrap();
        let prog = prog.into();
        traverse(&prog, &|n: &ast::Node| {
            let t: ast::NodeType = n.into();
            println!("node: {:?}", t);
        });
    }
}
