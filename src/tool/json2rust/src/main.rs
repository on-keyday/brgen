use ast2rust::ast2rust::ast;
use serde::{self, Deserialize};
mod generator;

fn main() {
    let mut deserializer = serde_json::Deserializer::from_reader(std::io::stdin());
    let file = ast::AstFile::deserialize(&mut deserializer);
    if let Err(e) = file {
        println!("error: {:?}", e);
        return;
    }
    let file = file.unwrap();
    let prog = ast::parse_ast(file.ast.unwrap());
    if let Err(e) = prog {
        println!("error: {:?}", e);
        return;
    }
    let gen = generator::Generator::new();
    let prog = prog.unwrap();
    gen.write_program(prog);
}
