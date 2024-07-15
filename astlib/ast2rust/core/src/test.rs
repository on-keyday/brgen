use std::{env, path};

use crate::ast;
use serde::Deserialize;

pub fn exec_and_output(file: &str) -> std::io::Result<std::process::Output> {
    env::set_current_dir(path::Path::new("..")).unwrap();
    println!("current dir: {:?}", env::current_dir().unwrap());
    let cmd = env::current_dir()
        .unwrap()
        .join(path::Path::new("tool/src2json"));
    #[cfg(target_os = "windows")]
    let cmd = cmd.with_extension("exe");
    println!("cmd: {:?}", cmd);
    let mut cmd = std::process::Command::new(cmd);
    cmd.arg(file).output()
}

pub fn load_test_file() -> ast::Node {
    let ch = exec_and_output("./example/feature_test/tree_test.bgn").unwrap();
    let mut de = serde_json::Deserializer::from_slice(&ch.stdout);
    let file = ast::AstFile::deserialize(&mut de).unwrap();
    let prog = ast::parse_ast(file.ast.unwrap()).unwrap();
    prog.into()
}
