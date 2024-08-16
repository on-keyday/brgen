use std::{fs, process::ExitCode};

mod target;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    let input = match std::fs::read(&args[1]) {
        Ok(input) => input,
        Err(e) => {
            eprintln!("Error reading file: {}", e);
            return ExitCode::from(2);
        }
    };
    let decoded = match target::TEST_CLASS::decode_exact(&input[..]) {
        Ok(x) => x,
        Err(e) => {
            eprintln!("Error decoding: {}", e);
            return ExitCode::from(1);
        }
    };
    let mut output = Vec::new();
    match decoded.encode(&mut output) {
        Ok(_) => (),
        Err(e) => {
            eprintln!("Error encoding: {}", e);
            return ExitCode::from(2);
        }
    }
    fs::write(&args[2], &output).unwrap();
    ExitCode::from(0)
}
