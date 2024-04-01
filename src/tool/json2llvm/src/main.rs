mod generate;
use ast2rust::ast;
use serde::Deserialize;
use std::process::ExitCode;

fn main() -> ExitCode {
    let mut deserializer = serde_json::Deserializer::from_reader(std::io::stdin());
    let file = ast::AstFile::deserialize(&mut deserializer);
    if let Err(e) = file {
        eprintln!("error: {:?}", e);
        return ExitCode::FAILURE;
    }
    let file = file.unwrap();
    let prog = ast::parse_ast(file.ast.unwrap());
    if let Err(e) = prog {
        eprintln!("error: {:?}", e);
        return ExitCode::FAILURE;
    }
    let prog = prog.unwrap();
    let mut gen = generate::Generator::new(&prog);
    gen.generate();
    ExitCode::SUCCESS
}
