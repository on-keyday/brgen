pub mod generator;
use ast2rust::ast;
use serde::Deserialize;
use wasm_bindgen::prelude::wasm_bindgen;

#[wasm_bindgen]
pub fn json2rust(input: &str) -> String {
    let mut deserializer = serde_json::Deserializer::from_str(input);
    let file = ast::AstFile::deserialize(&mut deserializer);
    if let Err(e) = file {
        return format!("error: {:?}", e);
    }
    let file = file.unwrap();
    let prog = ast::parse_ast(file.ast.unwrap());
    if let Err(e) = prog {
        return format!("error: {:?}", e);
    }
    let mut gen = generator::Generator::new(Vec::new());
    let prog = prog.unwrap();
    let b = gen.write_program(prog);
    if let Err(e) = b {
        return format!("error: {:?}", e);
    }
    let w = gen.get_mut_writer();
    return format!(
        "{} length: {}",
        String::from_utf8(w.clone()).unwrap(),
        w.len()
    );
    String::from_utf8(w.clone()).unwrap()
}
