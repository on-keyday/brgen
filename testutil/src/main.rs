use clap::{arg, Parser};
use serde::{Deserialize, Serialize};

#[derive(Parser)]
struct Args {
    #[arg(long, short('f'))]
    test_info_file: String,
}

#[derive(Serialize, Deserialize)]
struct GeneratedFile {
    dir: String,
    base: String,
    suffix: String,
}

struct TestInfo {
    total_count: u64,
    err_count: u64,
    time: String,
    generated_files: GeneratedFile,
}

fn main() {}
