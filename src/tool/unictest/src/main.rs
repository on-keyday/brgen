use clap::{arg, Parser};
use futures::{self, future};
use serde::Deserialize;
use std::{
    fs,
    path::{Path, PathBuf},
    sync::Arc,
};
use tokio::task::JoinHandle;
use uuid::Uuid;

#[derive(Parser)]
struct Args {
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

#[derive(Deserialize, Clone, Debug)]
pub struct TestRunner {
    // test runner name
    pub name: String,
    // setup source file
    pub source_setup_command: Vec<String>,
    // command to run test
    pub run_command: Vec<String>,

    #[serde(skip_deserializing)]
    pub file: Option<String>,
}

#[derive(Deserialize, Clone, Debug)]
pub struct TestInput {
    pub name: String,
    // input binary file
    pub binary: String,
    // test target format name
    pub format_name: String,
    // source file name that contains format_name format
    pub source: String,
    // this input is failure case
    pub failure_case: bool,
    // binary is actually hex string file
    pub hex: bool,
}

#[derive(Deserialize, Clone, Debug)]
pub struct TestRunnerFile {
    pub file: String,
}

#[derive(Deserialize, Clone, Debug)]
#[serde(untagged)]
pub enum RunnerConfig {
    TestRunner(TestRunner),
    File(TestRunnerFile),
}

#[derive(Deserialize, Clone)]
pub struct TestConfig {
    pub runners: Vec<RunnerConfig>,
    // test input binary file
    pub inputs: Vec<TestInput>,
}

fn compile_hex(input: Vec<u8>) -> anyhow::Result<Vec<u8>> {
    // skip space,tab,line and # line comment
    let mut hex = Vec::new();
    let mut i = 0;
    let mut pair: Option<u8> = None;
    let mut line = 1;
    let mut col = 0;
    while i < input.len() {
        let c = input[i];
        if c == b' ' || c == b'\t' || c == b'\n' || c == b'\r' {
            if c == b'\n' {
                line += 1;
                col = 0;
            }
            col += 1;
            i += 1;
            continue;
        }
        if c == b'#' {
            while i < input.len() && input[i] != b'\n' {
                i += 1;
            }
            continue;
        }
        let lsb = if b'0' <= c && c <= b'9' {
            c - b'0'
        } else if b'a' <= c && c <= b'f' {
            c - b'a' + 10
        } else if b'A' <= c && c <= b'F' {
            c - b'A' + 10
        } else {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("invalid hex string at {}:{}:{}", line, col, c as char),
            )
            .into());
        };
        if let Some(msb) = pair {
            hex.push(msb << 4 | lsb);
            pair = None
        } else {
            pair = Some(lsb)
        }
        i += 1;
        col += 1;
    }
    if let Some(x) = pair {
        Err(std::io::Error::new(
            std::io::ErrorKind::Other,
            format!("invalid hex string; missing pair for {x}"),
        )
        .into())
    } else {
        Ok(hex)
    }
}

struct InputBinaryCache {
    input_binaries: std::collections::HashMap<(PathBuf, bool), (PathBuf, Vec<u8>)>,
    tmp_dir: Option<PathBuf>,
}

fn path_str(path: &PathBuf) -> String {
    match path.to_str() {
        Some(x) => x.to_string(),
        None => path.to_string_lossy().to_string(),
    }
}

impl InputBinaryCache {
    fn new() -> Self {
        Self {
            input_binaries: std::collections::HashMap::new(),
            tmp_dir: None,
        }
    }

    fn read_input_binary(
        &mut self,
        path: &PathBuf,
        is_hex: bool,
    ) -> anyhow::Result<(PathBuf, Vec<u8>)> {
        let key = (path.clone(), is_hex);
        if let Some(x) = self.input_binaries.get(&key) {
            return Ok(x.clone());
        } else {
            let t = match fs::read(path) {
                Ok(x) => x,
                Err(x) => {
                    return Err(std::io::Error::new(
                        std::io::ErrorKind::Other,
                        format!("read file error: {:?}: {}", path, x),
                    )
                    .into());
                }
            };
            let t = if is_hex {
                let c = compile_hex(t)?;
                let tmp_path = self.get_tmp_dir()?;
                let tmp_path = tmp_path.join(path.file_name().unwrap());
                fs::write(tmp_path.clone(), &c)?;
                (tmp_path, c)
            } else {
                (path.clone(), t)
            };
            self.input_binaries.insert(key.clone(), t);
            Ok(self.input_binaries.get(&key).unwrap().clone())
        }
    }

    fn get_tmp_dir(&mut self) -> anyhow::Result<PathBuf> {
        if let Some(x) = &self.tmp_dir {
            Ok(x.clone())
        } else {
            let dir = std::env::temp_dir();
            let dir = dir.join("unictest");
            let dir = dir.join(format!("test-run-{}", Uuid::new_v4()));
            fs::create_dir_all(&dir)?;
            self.tmp_dir = Some(dir.clone());
            Ok(dir)
        }
    }

    fn runner_dir(&mut self) -> anyhow::Result<PathBuf> {
        let tmp_dir = self.get_tmp_dir()?;
        let runner_dir = tmp_dir.join(format!("runner-{}", Uuid::new_v4()));
        fs::create_dir_all(&runner_dir)?;
        Ok(runner_dir)
    }

    pub fn remove_tmp_dir(&self) {
        if let Some(dir) = &self.tmp_dir {
            fs::remove_dir_all(dir).unwrap();
        }
    }

    pub fn print_tmp_dir(&self) {
        if let Some(dir) = &self.tmp_dir {
            println!("tmp directory is {}", path_str(&dir));
        }
    }
}

struct PrepareInfo {
    runner: String,
    source: String,
    candidate_formats: Vec<String>,
    runner_dir: PathBuf,
    run_command: Vec<String>,
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let timer = std::time::Instant::now();
    let parsed = Args::parse();
    let test_config = fs::read_to_string(Path::new(&parsed.test_config_file))?;
    let mut d1 = serde_json::Deserializer::from_str(&test_config);
    let test_config = TestConfig::deserialize(&mut d1)?;
    // replace RunnerConfig::File with actual TestRunner
    let test_runner = test_config
        .runners
        .into_iter()
        .map(|runner| -> Result<TestRunner, anyhow::Error> {
            match runner {
                RunnerConfig::TestRunner(runner) => Ok(runner),
                RunnerConfig::File(runner_file) => {
                    let runner_config_str = fs::read_to_string(Path::new(&runner_file.file))?;
                    let mut d2 = serde_json::Deserializer::from_str(&runner_config_str);
                    let mut actual_runner: TestRunner = TestRunner::deserialize(&mut d2)?;
                    actual_runner.file = Some(runner_file.file.clone());
                    Ok(actual_runner)
                }
            }
        })
        .collect::<Result<Vec<_>, _>>()?;
    // replace $WORK_DIR in binary and source paths
    let inputs = test_config
        .inputs
        .into_iter()
        .map(|input| TestInput {
            name: input.name,
            binary: input
                .binary
                .replace("$WORK_DIR", &path_str(&std::env::current_dir().unwrap())),
            format_name: input.format_name,
            source: input
                .source
                .replace("$WORK_DIR", &path_str(&std::env::current_dir().unwrap())),
            failure_case: input.failure_case,
            hex: input.hex,
        })
        .collect::<Vec<_>>();
    let binary_cache = Arc::new(std::sync::Mutex::new(InputBinaryCache::new()));
    let mut runner_source_map = std::collections::HashMap::new();
    for input in &inputs {
        for runner in &test_runner {
            let key = (runner.name.clone(), input.source.clone());
            let entry = runner_source_map.entry(key);
            entry
                .and_modify(|x: &mut (Vec<String>, Vec<String>, Vec<String>)| {
                    if x.2.contains(&input.format_name) {
                        return;
                    }
                    x.2.push(input.format_name.clone());
                })
                .or_insert_with(|| {
                    (
                        runner.source_setup_command.clone(),
                        runner.run_command.clone(),
                        vec![input.format_name.clone()],
                    )
                });
        }
    }

    let mut handles = Vec::new();

    let (send, mut recv) = tokio::sync::mpsc::channel(100);

    for ((runner_name, source_name), (setup_command, run_command, candidate_formats)) in
        runner_source_map.into_iter()
    {
        let cache = binary_cache.clone();
        let send = send.clone();

        let h: JoinHandle<anyhow::Result<()>> = tokio::spawn(async move {
            println!(
                "Running test setup: runner name={}, source={}",
                runner_name, source_name
            );
            let runner_dir = {
                let mut cache = cache.lock().unwrap();
                cache.runner_dir()?
            };
            if setup_command.is_empty() {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    format!(
                        "runner.source_setup_command is empty: runner name={}",
                        runner_name
                    ),
                )
                .into());
            }
            // replace $WORK_DIR to current working dir
            let replaced_setup_command: Vec<String> = setup_command
                .iter()
                .map(|s| s.replace("$WORK_DIR", &path_str(&std::env::current_dir().unwrap())))
                .collect();
            let spawned = tokio::process::Command::new(replaced_setup_command[0].clone())
                .args(&replaced_setup_command[1..])
                .current_dir(&runner_dir)
                .env(
                    "UNICTEST_ORIGINAL_WORK_DIR",
                    &path_str(&std::env::current_dir().unwrap()),
                )
                .env("UNICTEST_SOURCE_FILE", &source_name)
                .env("UNICTEST_RUNNER_DIR", &runner_dir)
                .env("UNICTEST_RUNNER_NAME", &runner_name)
                .env("UNICTEST_CANDIDATE_FORMATS", &candidate_formats.join(","))
                .output()
                .await?;
            send.send((
                spawned,
                PrepareInfo {
                    runner: runner_name,
                    source: source_name,
                    candidate_formats,
                    runner_dir,
                    run_command,
                },
            ))
            .await?;
            Ok(())
        });
        handles.push(h);
    }
    drop(send);
    let mut setup_infos = Vec::new();
    while let Some((output, prepare_info)) = recv.recv().await {
        println!(
            "Test setup completed: runner name={}, source={}",
            prepare_info.runner, prepare_info.source
        );
        println!(
            "STDOUT:\n########\n{}\n########\n",
            String::from_utf8_lossy(&output.stdout)
        );
        println!(
            "STDERR:\n########\n{}\n########\n",
            String::from_utf8_lossy(&output.stderr)
        );
        println!("status: {}", output.status);
        if !output.status.success() {
            println!("Test setup failed, skipping tests for this setup.");
        } else {
            setup_infos.push(prepare_info);
        }
        println!("----------------------------------------");
    }

    future::try_join_all(handles).await?;

    println!("Setup all tests in {:?}", timer.elapsed());

    let mut test_handles = Vec::new();
    for p in setup_infos {
        println!(
            "Ready to run tests: runner name={}, source={}",
            p.runner, p.source
        );
        println!("Candidate formats: {:?}", p.candidate_formats);
        println!("Runner dir: {:?}", p.runner_dir);
        println!("Run command: {:?}", p.run_command);
        println!("----------------------------------------");

        for input in &inputs {
            if !p.candidate_formats.contains(&input.format_name) {
                continue;
            }
            let input = input.clone();
            let cache = binary_cache.clone();
            let runner_command = p.run_command.clone();
            let source = p.source.clone();
            let runner_dir = p.runner_dir.clone();
            let runner = p.runner.clone();
            let h: JoinHandle<anyhow::Result<()>> = tokio::spawn(async move {
                let (input_path, _input_data) = cache
                    .lock()
                    .unwrap()
                    .read_input_binary(&PathBuf::from(&input.binary), input.hex)?;
                println!(
                    "Running test: runner={}, input={} source={}, format={}, binary={} hex={}",
                    runner, input.name, source, input.format_name, input.binary, input.hex
                );
                if runner_command.is_empty() {
                    return Err(std::io::Error::new(
                        std::io::ErrorKind::Other,
                        format!(
                            "run_command is empty: runner={}, input={}",
                            runner, input.name
                        ),
                    )
                    .into());
                }
                let task_dir = runner_dir.join(format!(
                    "task-{}-{}-{}",
                    input.name,
                    input.format_name,
                    Uuid::new_v4()
                ));
                fs::create_dir_all(&task_dir)?;
                let replaced_run_command: Vec<String> = runner_command
                    .iter()
                    .map(|s| s.replace("$WORK_DIR", &path_str(&std::env::current_dir().unwrap())))
                    .collect();
                tokio::process::Command::new(replaced_run_command[0].clone())
                    .args(&replaced_run_command[1..])
                    .current_dir(&task_dir)
                    .env(
                        "UNICTEST_ORIGINAL_WORK_DIR",
                        &path_str(&std::env::current_dir().unwrap()),
                    )
                    .env("UNICTEST_SOURCE_FILE", &source)
                    .env("UNICTEST_WORK_DIR", &task_dir)
                    .env("UNICTEST_RUNNER_DIR", &runner_dir)
                    .env("UNICTEST_RUNNER_NAME", &runner)
                    .env("UNICTEST_BINARY_FILE", &input_path)
                    .env("UNICTEST_INPUT_NAME", &input.name)
                    .env("UNICTEST_INPUT_FORMAT", &input.format_name)
                    .env(
                        "UNICTEST_FAILURE_CASE",
                        if input.failure_case { "1" } else { "0" },
                    )
                    .output()
                    .await
                    .and_then(|output| {
                        println!(
                            "Test completed: runner name={}, source={}, format={}, input={}",
                            runner, source, input.format_name, input.binary
                        );
                        println!(
                            "STDOUT:\n########\n{}\n########\n",
                            String::from_utf8_lossy(&output.stdout)
                        );
                        println!(
                            "STDERR:\n########\n{}\n########\n",
                            String::from_utf8_lossy(&output.stderr)
                        );
                        println!("status: {}", output.status);

                        if output.status.success() && input.failure_case {
                            println!("expected failure case, but succeeded");
                        } else if !output.status.success() && !input.failure_case {
                            println!("expected success case, but failed");
                        }
                        println!("----------------------------------------");
                        Ok(())
                    })?;
                Ok(())
            });
            test_handles.push(h);
        }
    }
    futures::future::try_join_all(test_handles).await?;
    println!("All tests completed in {:?}", timer.elapsed());
    if parsed.save_tmp_dir {
        let cache = binary_cache.lock().unwrap();
        cache.print_tmp_dir();
    } else {
        let cache = binary_cache.lock().unwrap();
        cache.remove_tmp_dir();
    }
    if parsed.clean_tmp {
        let tmp_dir = std::env::temp_dir().join("unictest");
        if tmp_dir.exists() {
            if parsed.save_tmp_dir {
                let current_tmp = binary_cache.lock().unwrap().tmp_dir.clone();
                // remove all except current tmp dir
                for entry in fs::read_dir(tmp_dir)? {
                    let entry = entry?;
                    let path = entry.path();
                    if path.is_dir() && Some(path.clone()) != current_tmp {
                        fs::remove_dir_all(&path)?;
                        println!("Removed temporary directory {}", path_str(&path));
                    }
                }
            } else {
                fs::remove_dir_all(&tmp_dir)?;
                println!("Removed temporary directory {}", path_str(&tmp_dir));
            }
        }
    }
    Ok(())
}
