use std::{fs, path::Path};

use clap::{arg, Parser};
use serde::Deserialize;
use testutil::{Error, TestSchedule};
mod testutil;

#[derive(Parser)]
struct Args {
    #[arg(long, short('f'))]
    test_info_file: String,
    #[arg(long, short('c'))]
    test_config_file: String,
}
fn main() -> Result<(), Error> {
    let parsed = Args::parse();
    let test_config =fs::read_to_string(Path::new (&parsed.test_config_file))?;
    let mut d1 = serde_json::Deserializer::from_str(&test_config);
    let test_config = testutil::TestConfig::deserialize(&mut d1)?;
    let mut ext_set = std::collections::HashMap::new();
    let mut file_format_list = std::collections::HashMap::new();
    for runner in &test_config.runners {
        let mut has_input = false;
        let mut has_output = false;
        let mut has_exec = false;
        for cmd in &runner.build_command {
            if cmd == "$INPUT" {
                has_input = true;
            }
            if cmd == "$OUTPUT" {
                has_output = true;
            }
        }
        if !has_input || !has_output {
            eprint!(
                "build command not contains $INPUT or $OUTPUT; should contain both {:?}",
                runner.build_command
            );
            continue;
        }
        has_input = false;
        has_output = false;
        for cmd in &runner.run_command {
            if cmd == "$INPUT" {
                has_input = true;
            }
            if cmd == "$OUTPUT" {
                has_output = true;
            }
            if cmd == "$EXEC" {
                has_exec = true;
            }
        }
        if !has_input || !has_output || !has_exec {
            eprint!(
                "run command not contains $INPUT,$OUTPUT or $EXEC; should contain all {:?}",
                runner.run_command
            );
            continue;
        }
        ext_set.insert(runner.suffix.clone(), runner.clone());
    }
    for input in &test_config.inputs {
        file_format_list.insert(
            (input.file_base.clone(), input.format_name.clone()),
            input.clone(),
        );
    }
    let mut sched = Vec::new();
    let test_info = fs::read_to_string(Path::new(&parsed.test_info_file))?;
    let mut d2 = serde_json::Deserializer::from_str(&test_info);
    let test_info = testutil::TestInfo::deserialize(&mut d2)?;
    for file in &test_info.generated_files {
        if file.suffix != ".json" {
            continue;
        }
        let split = file.base.split('.').collect::<Vec<&str>>();
        if split.len() != 2 {
            continue;
        }
        let base = split[0];
        let ext = split[1];
        let runner = match ext_set.get(ext) {
            Some(x) => x,
            None => continue,
        };
        let path = file.into_path();
        let content = fs::read_to_string(&path)?;
        let mut d = serde_json::Deserializer::from_str(&content);
        let data = match testutil::GeneratedData::deserialize(&mut d) {
            Ok(file) => file,
            Err(e) => {
                eprintln!("failed to deserialize {}: {}", path, e);
                continue;
            }
        };
        for s in &data.structs {
            if let Some(input) = file_format_list.get(&(base.to_string(), s.clone())) {
                sched.push(TestSchedule {
                    runner: runner,
                    input: input,
                    file: file,
                })
            }
        }
    }
    let mut scheduler = testutil::TestScheduler::new();
    for s in sched {
        match scheduler.run_test_schedule(&s) {
            Ok(()) => println!(
                "test passed: {:?}",
                s.file.into_path() + &s.input.format_name
            ),
            Err(x) => println!(
                "test failed: {:?} {:?}",
                s.file.into_path() + &s.input.format_name,
                x
            ),
        }
    }
    Ok(())
}
