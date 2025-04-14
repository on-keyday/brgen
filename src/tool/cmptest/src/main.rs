use std::{borrow::BorrowMut, fs, path::Path, sync::Arc};

use ast2rust::ast::GenerateMapFile;
use clap::{arg, Parser};
use serde::Deserialize;
use testutil::{Error, TestSchedule};
mod testutil;
use colored::Colorize;

#[derive(Parser)]
struct Args {
    #[arg(long, short('f'), help("test info file (json)"))]
    test_info_file: String,
    #[arg(long, short('c'), help("test config file (json)"))]
    test_config_file: String,
    #[arg(long, short('d'), help("debug mode"))]
    debug: bool,
    #[arg(long, help("save tmp dir"))]
    save_tmp_dir: bool,
    #[arg(long, help("print fail command"))]
    print_fail_command: bool,
    #[arg(
        long("as-vscode"),
        help("print fail command as vscode launch.json compatible")
    )]
    vscode: bool,
    #[arg(long, help("expected test count that will be loaded"))]
    expected_test_total: Option<usize>,
    #[arg(
        long,
        help("clean temporary directory (except current temporary directory if --save-tmp-dir specified) (remove cmptest-* in $TMP)")
    )]
    clean_tmp: bool,
    #[arg(long, help("print stdout of test (for debug)"))]
    print_stdout: bool,
}

#[tokio::main]
async fn main() -> Result<(), Error> {
    let timer = std::time::Instant::now();
    let parsed = Args::parse();
    let test_config = fs::read_to_string(Path::new(&parsed.test_config_file))?;
    let mut d1 = serde_json::Deserializer::from_str(&test_config);
    let mut test_config = testutil::TestConfig::deserialize(&mut d1)?;
    let mut ext_set = std::collections::HashMap::new();
    let mut file_format_list: std::collections::HashMap<
        (String, String),
        Vec<Arc<testutil::TestInput>>,
    > = std::collections::HashMap::new();
    for runner_config in &mut test_config.runners {
        let runner = match runner_config {
            testutil::RunnerConfig::TestRunner(x) => x,
            testutil::RunnerConfig::File(x) => {
                let filename = x.file.clone();
                let r = fs::read(&filename)?;
                let mut de = serde_json::Deserializer::from_slice(&r);
                let r = testutil::TestRunner::deserialize(&mut de)?;
                *runner_config = testutil::RunnerConfig::TestRunner(r);
                if let testutil::RunnerConfig::TestRunner(x) = runner_config {
                    x.file = Some(filename);
                    x
                } else {
                    unreachable!()
                }
            }
        };
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
        if parsed.debug {
            eprint!("runner: {:?}", runner)
        }
        ext_set.insert(runner.suffix.clone(), Arc::new(runner.clone()));
    }
    for input in &test_config.inputs {
        let key = (input.file_base.clone(), input.format_name.clone());
        match file_format_list.get_mut(&key) {
            Some(x) => {
                x.push(Arc::new(input.clone()));
            }
            None => {
                file_format_list.insert(key, vec![Arc::new(input.clone())]);
            }
        }
    }
    let mut sched = Vec::new();
    let test_info = fs::read_to_string(Path::new(&parsed.test_info_file))?;
    let mut d2 = serde_json::Deserializer::from_str(&test_info);
    let test_info = testutil::TestInfo::deserialize(&mut d2)?;
    for file in test_info.generated_files {
        if parsed.debug {
            eprintln!("file: {:?}", file);
        }
        if file.suffix != ".json" {
            continue;
        }
        let split = file.base.split('.').collect::<Vec<&str>>();
        if split.len() != 2 {
            continue;
        }
        let base = split[0].to_string();
        let ext = split[1];
        let runner = match ext_set.get(ext) {
            Some(x) => x,
            None => {
                if parsed.debug {
                    eprintln!("runner not found for ext: {}", ext);
                }
                continue;
            }
        };

        let path = file.into_path();
        let content = match fs::read_to_string(&path) {
            Ok(x) => x,
            Err(x) => {
                eprintln!("file load error: {}: {}", path, x);
                continue;
            }
        };
        let mut d = serde_json::Deserializer::from_str(&content);
        let data = match GenerateMapFile::deserialize(&mut d) {
            Ok(file) => file,
            Err(e) => {
                eprintln!("failed to deserialize {}: {}", path, e);
                continue;
            }
        };
        let file = Arc::new(file);
        for s in &data.structs {
            let key = (base.clone(), s.clone());
            if parsed.debug {
                eprintln!("key: {:?}", key);
            }
            if let Some(input) = file_format_list.get(&key) {
                if parsed.debug {
                    eprintln!("input matched: {:?}", input);
                }
                for input in input {
                    sched.push(TestSchedule {
                        runner: runner.clone(),
                        input: input.clone(),
                        file: file.clone(),
                    });
                }
            }
        }
    }
    let mut scheduler = testutil::TestScheduler::new(parsed.debug);
    let mut failed = 0;
    let (send, mut recv) = tokio::sync::mpsc::channel(1);
    let total = sched.len();

    if let Some(x) = parsed.expected_test_total {
        if total != x {
            return Err(testutil::Error::TestFail(format!(
                "expect {} test load but loaded test are {}: {:?}",
                x,
                total,
                sched.iter().map(|x| x.test_name()).collect::<Vec<_>>()
            ))
            .into());
        }
    }
    let print_fail = |x: &testutil::Error| match x {
        testutil::Error::TestFail(s) => {
            eprintln!("reason: TestFail {}", s);
        }
        testutil::Error::Exec(x) => {
            eprintln!("reason: Exec {}", x);
        }
        testutil::Error::IO(x) => {
            eprintln!("reason: IO {:?}", x);
        }
        testutil::Error::JSON(x) => {
            eprintln!("reason: JSON {:?}", x);
        }
        testutil::Error::Join(x) => {
            eprintln!("reason: Join {:?}", x);
        }
    };
    for s in sched {
        match scheduler.run_test_schedule(&s, send.clone()) {
            Ok(_) => {
                println!("test {} scheduled...", s.test_name());
            }
            Err(x) => {
                eprintln!("{}: {}", "FAIL".red(), s.test_name());
                print_fail(&x);
                failed += 1;
            }
        }
    }
    drop(send);
    while let Some(x) = recv.recv().await {
        match x {
            Ok((x, stdout)) => {
                println!("{}: {}", "PASS".green(), x.test_name());
                if parsed.print_stdout {
                    println!("stdout:\n{}", stdout);
                }
            }
            Err((sched, stdout, err, cmdline)) => {
                eprintln!("{}: {}", "FAIL".red(), sched.test_name());
                print_fail(&err);
                if parsed.print_fail_command {
                    if parsed.vscode {
                        eprintln!(
                            r#"command line: {{"name":{},"program":{},"args":{}}}"#,
                            serde_json::to_string(&sched.test_name()).unwrap(),
                            serde_json::to_string(&cmdline[0]).unwrap(),
                            serde_json::to_string(&cmdline[1..]).unwrap()
                        );
                    } else {
                        eprintln!("command line: {:?}", cmdline);
                    }
                }
                if parsed.print_stdout {
                    eprintln!("stdout:\n{}", stdout);
                }
                failed += 1;
            }
        }
    }
    println!(
        "Result: Total:{} {}:{} {}:{}",
        total,
        "PASS".green(),
        total - failed,
        "FAIL".red(),
        failed
    );
    println!("Time: {:?}", timer.elapsed());
    let mut current_save_dir = None;
    if parsed.save_tmp_dir {
        current_save_dir = scheduler.print_tmp_dir();
    } else {
        scheduler.remove_tmp_dir();
    }
    if parsed.clean_tmp {
        fs::read_dir(std::env::temp_dir())?
            .filter_map(|x| x.ok())
            .filter(|x| {
                x.file_name()
                    .to_str()
                    .map(|x| {
                        if x.starts_with("cmptest-") {
                            if let Some(y) = current_save_dir.borrow_mut() {
                                let y = y.file_name().unwrap().to_string_lossy();
                                if x == y {
                                    return false; // skip current tmp dir
                                }
                            }
                            return true;
                        }
                        false
                    })
                    .unwrap_or(false)
            })
            .for_each(|x| {
                if let Err(e) = fs::remove_dir_all(x.path()) {
                    eprintln!("failed to remove {}: {}", x.path().to_string_lossy(), e);
                } else {
                    println!("removed: {:?}", x.path());
                }
            });
    }
    if failed > 0 {
        return Err(testutil::Error::TestFail("some tests failed".to_string()).into());
    }
    Ok(())
}
