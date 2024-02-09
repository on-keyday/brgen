use std::{io::stdout, process::ExitCode};

use ast2rust::ast;
use clap::{arg, Parser};

use serde::{self, Deserialize};
mod generator;

#[derive(Parser)]
struct Args {
    #[arg(long, short)]
    spec: bool,
}

fn main() -> ExitCode {
    let matches = Args::parse();
    if matches.spec {
        const SPEC: &str = r#####################"
        {
            "input": "stdin",
            "langs": ["rust","json"],
            "suffix": [".rs",".json"],
            "separator": "#############\n"
        }
    "#####################;
        println!("{}", SPEC);
        return ExitCode::SUCCESS;
    }
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
    let mut gen = generator::Generator::new(stdout());
    let prog = prog.unwrap();
    let b = gen.write_program(prog);
    if let Err(e) = b {
        eprintln!("error: {:?}", e);
        return ExitCode::FAILURE;
    }
    ExitCode::SUCCESS
}
