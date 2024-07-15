use std::{
    io::{stdout, Read},
    process::ExitCode,
};

use anyhow::Error;
use ast2rust::ast;
use clap::{arg, Parser};

use serde::{self, Deserialize};
mod generator;

#[derive(Parser)]
struct Args {
    #[arg(long, short)]
    spec: bool,
    #[arg(long, short)]
    file: Option<String>,
}

fn switch_file(file: Option<String>) -> Result<Vec<u8>, Error> {
    if let Some(file) = file {
        let file = std::fs::File::open(&file)?;
        let mut reader = std::io::BufReader::new(file);
        let mut buf = Vec::new();
        reader.read_to_end(&mut buf)?;
        Ok(buf)
    } else {
        let mut buf = Vec::new();
        std::io::stdin().read_to_end(&mut buf)?;
        Ok(buf)
    }
}

fn main() -> ExitCode {
    let matches = Args::parse();
    if matches.spec {
        const SPEC: &str = r#####################"
        {
            "input": "stdin",
            "langs": ["rust","json"],
            "suffix": [".rs",".rs.json"],
            "separator": "#############\n"
        }
    "#####################;
        println!("{}", SPEC);
        return ExitCode::SUCCESS;
    }
    let file = switch_file(matches.file).map_err(|e| {
        eprintln!("error: {:?}", e);
        ExitCode::FAILURE
    });
    let file = match file {
        Ok(file) => file,
        Err(e) => return e,
    };
    let mut deserializer = serde_json::Deserializer::from_slice(&file);
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
    println!("{}", "#############\n");
    let js = serde_json::to_writer(stdout(), &gen.map_file);
    if let Err(e) = js {
        eprintln!("error: {:?}", e);
        return ExitCode::FAILURE;
    }
    ExitCode::SUCCESS
}
