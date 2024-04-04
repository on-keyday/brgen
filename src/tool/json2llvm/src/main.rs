mod generate;
use ast2rust::ast;
use generate::Error;
use serde::Deserialize;
use std::process::ExitCode;

fn main() -> Result<(), Error> {
    let mut deserializer = serde_json::Deserializer::from_reader(std::io::stdin());
    let file = ast::AstFile::deserialize(&mut deserializer);
    if let Err(e) = file {
        return Err(Error::JSONError(e));
    }
    let file = file.unwrap();
    let prog = ast::parse_ast(file.ast.unwrap());
    if let Err(e) = prog {
        return Err(Error::ParseError(e));
    }
    let prog = prog.unwrap();
    let gen = generate::Generator::new(&prog);
    gen.generate()?;
    Ok(())
}
