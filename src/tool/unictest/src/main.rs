use clap::Parser;
use colored::Colorize;
use futures::{self, future};
use serde::{Deserialize, Serialize};
use std::{
    fs,
    path::{Path, PathBuf},
    thread,
};
use tokio::task::JoinHandle;
use uuid::Uuid;
#[derive(Parser)]
struct Args {
    #[arg(long, short('c'), help("test config file (json)"))]
    test_config_file: String,
    #[arg(long, help("save current executed temporary directory"))]
    save_tmp_dir: bool,
    #[arg(long, help("expected test count that will be loaded"))]
    expected_test_total: Option<usize>,
    #[arg(
        long,
        help("clean temporary directory (except current temporary directory if --save-tmp-dir specified) (remove unictest/* in $TMP)")
    )]
    clean_tmp: bool,
    #[arg(long, help("print stdout of test (for debug)"))]
    print_stdout: bool,

    #[arg(
        long,
        help(
            "verbose output (only specifying to test script, not used in unictest command itself)"
        )
    )]
    verbose: bool,

    #[arg(
        long,
        help("use shorter path for temporary directory (for Windows cmd.exe limitation)")
    )]
    shorter_path: bool,

    #[arg(long, help("target runner"),action = clap::ArgAction::Append)]
    target_runner: Vec<String>,

    #[arg(long, help("target input"),action = clap::ArgAction::Append)]
    target_input: Vec<String>,

    #[arg(long,help("test output json file path, if specified, test results will be saved in this file in JSON format"))]
    output_json: Option<String>,

    #[arg(long, help("parallel test execution limit. default is unlimited"))]
    parallel_limit: Option<usize>,

    #[arg(long, help("enable fuzz mode: generate random inputs via ebm2rmw and test for crashes"))]
    fuzz: bool,

    #[arg(long, help("seed for fuzz input generation (0 or unset = auto from time)"), requires("fuzz"))]
    fuzz_seed: Option<u64>,
}

#[derive(Deserialize, Clone, Debug)]
pub struct OptionSet {
    pub name: String,
    pub setup_options: Vec<String>,
    pub run_options: Vec<String>,
}

#[derive(Deserialize, Clone, Debug)]
pub struct TestRunner {
    // test runner name
    pub name: String,
    // setup source file
    pub source_setup_command: Vec<String>,
    // command to run test
    pub run_command: Vec<String>,

    // additional option sets for this runner
    pub option_sets: Option<Vec<OptionSet>>,

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
pub struct TestFile {
    pub file: String,
}

#[derive(Deserialize, Clone, Debug)]
#[serde(untagged)]
pub enum RunnerConfig {
    TestRunner(TestRunner),
    File(TestFile),
}

#[derive(Deserialize, Clone, Debug)]
#[serde(untagged)]
pub enum InputConfig {
    TestInput(Vec<TestInput>),
    File(TestFile),
}

#[derive(Deserialize, Clone)]
pub struct TestConfig {
    // common source code setup commands
    pub common_source_setup: Option<Vec<String>>,
    // test runners
    pub runners: Vec<RunnerConfig>,
    // test input binary file
    pub inputs: InputConfig,
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
            return Err(anyhow::anyhow!(
                "invalid hex string at line {}, column {}: invalid character {}",
                line,
                col,
                c as char
            ));
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
        Err(anyhow::anyhow!(
            "invalid hex string at line {}, column {}: odd number of hex digits, missing pair for {}",
            line,
            col,
            x
        ))
    } else {
        Ok(hex)
    }
}

struct InputBinaryCache {
    input_binaries: std::collections::HashMap<(PathBuf, bool), PathBuf>,
    tmp_dir: Option<PathBuf>,
    shorter: bool,
}

fn path_str(path: &PathBuf) -> String {
    match path.to_str() {
        Some(x) => x.to_string(),
        None => path.to_string_lossy().to_string(),
    }
}

impl InputBinaryCache {
    fn new(shorter: bool) -> Self {
        Self {
            input_binaries: std::collections::HashMap::new(),
            tmp_dir: None,
            shorter,
        }
    }

    fn read_input_binary(&mut self, path: &PathBuf, is_hex: bool) -> anyhow::Result<PathBuf> {
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
                let tmp_path = tmp_path.join(
                    path.file_name()
                        .ok_or_else(|| anyhow::anyhow!("invalid file name: {}", path.display()))?,
                );
                fs::write(tmp_path.clone(), &c)?;
                tmp_path
            } else {
                path.clone()
            };
            self.input_binaries.insert(key.clone(), t);
            Ok(self.input_binaries.get(&key).unwrap().clone())
        }
    }

    fn get_short_random_string() -> String {
        // 5 character random string
        Uuid::new_v4()
            .to_bytes_le()
            .iter()
            .skip(10)
            .take(5)
            .map(|b| format!("{:02x}", b))
            .collect::<String>()
    }

    fn get_tmp_dir(&mut self) -> anyhow::Result<PathBuf> {
        if let Some(x) = &self.tmp_dir {
            Ok(x.clone())
        } else {
            let dir = std::env::temp_dir();
            let dir = dir.join("unictest");
            let dir = if self.shorter {
                // 5 character random string
                let rand_str = Self::get_short_random_string();
                dir.join(format!("tr-{}", rand_str))
            } else {
                dir.join(format!("test-run-{}", Uuid::new_v4()))
            };
            fs::create_dir_all(&dir)?;
            self.tmp_dir = Some(dir.clone());
            Ok(dir)
        }
    }

    fn runner_dir(&mut self) -> anyhow::Result<PathBuf> {
        let tmp_dir = self.get_tmp_dir()?;
        let runner_dir = if self.shorter {
            let rand_str = Self::get_short_random_string();
            tmp_dir.join(format!("r-{}", rand_str))
        } else {
            tmp_dir.join(format!("runner-{}", Uuid::new_v4()))
        };
        fs::create_dir_all(&runner_dir)?;
        Ok(runner_dir)
    }

    pub fn remove_tmp_dir(&self) {
        if let Some(dir) = &self.tmp_dir {
            if let Err(e) = fs::remove_dir_all(dir) {
                eprintln!(
                    "Warning: Failed to remove temporary directory {}: {}",
                    path_str(dir),
                    e
                );
            }
        }
    }

    pub fn print_tmp_dir(&self) {
        if let Some(dir) = &self.tmp_dir {
            println!("tmp directory is {}", path_str(&dir));
        }
    }
}

#[derive(Clone)]
struct PrepareInfo {
    runner: String,
    option_set: OptionSet,
    source: String,
    candidate_formats: Vec<String>,
    runner_dir: PathBuf,
    run_command: Vec<String>,
}

#[derive(Clone)]
struct ResultInfo {
    runner: String,
    option_set: String,
    source: String,
    format_name: String,
    input_name: String,
    output: Option<std::process::Output>,
    failure_case: bool,
}

#[derive(Serialize, Clone, Debug)]
struct TestCaseResult {
    runner: String,
    option_set: String,
    source: String,
    format_name: String,
    input_name: String,
    success: bool,
    failure_case: bool,
    output_stdout: String,
    output_stderr: String,
    output_status: i32,
    status_interpretation: String,
    error_message: Option<String>,
}

#[derive(Serialize, Clone, Debug)]
struct TestResults {
    success_count: usize,
    fail_count: usize,
    setup_fail_count: usize,
    setup_failures: Vec<SetupFailureInfo>,
    results: Vec<TestCaseResult>,
}

const CONFIRMED_DECODER_FAILURE_EXIT_CODE: i32 = 10;
const UNEXPECTED_ENCODER_FAILURE_EXIT_CODE: i32 = 20;

pub fn print_output(output: &std::process::Output, print_stdout: bool) {
    if print_stdout {
        println!(
            "STDOUT:\n########\n{}\n########\n",
            String::from_utf8_lossy(&output.stdout)
        );
        println!(
            "STDERR:\n########\n{}\n########\n",
            String::from_utf8_lossy(&output.stderr)
        );
    }
    println!("status: {}", output.status);
}

struct MatrixEntry {
    setup_command: Vec<String>,
    run_command: Vec<String>,
    candidate_formats: Vec<String>,
    option_set: OptionSet,
}

#[derive(Hash, Eq, PartialEq, Clone, Debug)]
struct RunnerInputKey {
    runner_name: String,
    option_set_name: String,
    source_name: String,
}

type RunnerInputMatrix = std::collections::HashMap<RunnerInputKey, MatrixEntry>;

type SourceCodeFormatMap = std::collections::HashMap<String, Vec<String>>;

fn make_runner_input_matrix(
    runners: &Vec<TestRunner>,
    inputs: &Vec<TestInput>,
) -> (RunnerInputMatrix, SourceCodeFormatMap) {
    let mut runner_source_map = std::collections::HashMap::new();
    let mut source_format_map: SourceCodeFormatMap = std::collections::HashMap::new();
    for input in inputs {
        let entry = source_format_map
            .entry(input.source.clone())
            .or_insert_with(Vec::new);
        if !entry.contains(&input.format_name) {
            entry.push(input.format_name.clone());
        }
        for runner in runners {
            for option_set in runner.option_sets.as_ref().unwrap() {
                let key = RunnerInputKey {
                    runner_name: runner.name.clone(),
                    option_set_name: option_set.name.clone(),
                    source_name: input.source.clone(),
                };
                let entry = runner_source_map.entry(key);
                entry
                    .and_modify(|x: &mut MatrixEntry| {
                        if x.candidate_formats.contains(&input.format_name) {
                            return;
                        }
                        x.candidate_formats.push(input.format_name.clone());
                    })
                    .or_insert_with(|| MatrixEntry {
                        setup_command: runner.source_setup_command.clone(),
                        run_command: runner.run_command.clone(),
                        option_set: option_set.clone(),
                        candidate_formats: vec![input.format_name.clone()],
                    });
            }
        }
    }
    (runner_source_map, source_format_map)
}

async fn run_source_setup(
    sema: &Option<std::sync::Arc<tokio::sync::Semaphore>>,
    common_setup_commands: Vec<String>,
    runner_source_map: SourceCodeFormatMap,
    source_runner_matrix: &mut RunnerInputMatrix,
    binary_cache: &mut InputBinaryCache,
    current_dir: &String,
    parsed: &Args,
) -> anyhow::Result<(
    Vec<SetupFailureInfo>,
    std::collections::HashMap<String, String>,
)> {
    let replaced_setup_command: Vec<String> = common_setup_commands
        .iter()
        .map(|s| s.replace("$WORK_DIR", &current_dir))
        .collect();
    let mut common_setup_handles = Vec::new();
    let mut source_dir_map = std::collections::HashMap::new();
    let verbose = parsed.verbose;
    let (source_send, mut source_recv) = tokio::sync::mpsc::channel(100);
    for (source_name, candidate_formats) in runner_source_map.into_iter() {
        let source_dir = binary_cache.runner_dir()?;
        source_dir_map.insert(source_name.clone(), path_str(&source_dir));
        println!("Running common source setup: source={}", source_name);
        println!("-----------------------------------------");
        let replaced_setup_command = replaced_setup_command.clone();
        let source_name = source_name.clone();
        let candidate_formats = candidate_formats.clone();
        let source_dir = source_dir.clone();
        let current_dir = current_dir.clone();
        let source_send = source_send.clone();
        let sema = sema.clone();
        let h = tokio::spawn(async move {
            #[allow(unused_variables)]
            let bound = if let Some(sema) = &sema {
                let permit = match sema.acquire().await {
                    Ok(permit) => permit,
                    Err(e) => {
                        return Err(anyhow::anyhow!("failed to acquire semaphore permit for source setup: source={}, error={}", source_name, e));
                    }
                };
                Some(permit)
            } else {
                None
            };
            let spawned = tokio::process::Command::new(replaced_setup_command[0].clone())
                .args(&replaced_setup_command[1..])
                .current_dir(&source_dir)
                .env("UNICTEST_VERBOSE", if verbose { "1" } else { "0" })
                .env("UNICTEST_ORIGINAL_WORK_DIR", &current_dir)
                .env("UNICTEST_SOURCE_FILE", &source_name)
                .env("UNICTEST_SOURCE_SETUP_DIR", &source_dir)
                .env("UNICTEST_CANDIDATE_FORMATS", &candidate_formats.join(","))
                .output()
                .await?;
            source_send.send((spawned, source_name)).await?;
            Ok::<(), anyhow::Error>(())
        });
        common_setup_handles.push(h);
    }
    drop(source_send);
    let mut setup_failures = Vec::new();
    while let Some((output, source_name)) = source_recv.recv().await {
        println!("Common source setup completed: source={}", source_name);
        print_output(&output, parsed.print_stdout);
        if !output.status.success() {
            println!("Common source setup failed for source={}", source_name);
            // remove all entries with this source from source_runner_matrix
            source_runner_matrix.retain(|key, _| key.source_name != source_name);
            setup_failures.push(SetupFailureInfo::CommandFailure {
                source: source_name.clone(),
                runner: "common_source_setup".to_string(),
                option_set: "".to_string(),
                output_stdout: String::from_utf8_lossy(&output.stdout).into_owned(),
                output_stderr: String::from_utf8_lossy(&output.stderr).into_owned(),
            });
        }
        println!("----------------------------------------");
    }
    let result = futures::future::try_join_all(common_setup_handles).await?;

    for res in result {
        if let Err(e) = res {
            println!("Error during common source setup: {}", e);
            setup_failures.push(SetupFailureInfo::OrchestrationError {
                error_message: format!("orchestration error during common source setup: {}", e),
            });
        }
    }

    Ok((setup_failures, source_dir_map))
}

#[derive(Serialize, Clone, Debug)]
enum SetupFailureInfo {
    CommandFailure {
        source: String,
        option_set: String,
        runner: String,
        output_stdout: String,
        output_stderr: String,
    },
    OrchestrationError {
        error_message: String,
    },
}

async fn run_setup(
    sema: &Option<std::sync::Arc<tokio::sync::Semaphore>>,
    runner_source_map: RunnerInputMatrix,
    source_dir_map: Option<std::collections::HashMap<String, String>>,
    binary_cache: &mut InputBinaryCache,
    current_dir: &String,
    parsed: &Args,
    fuzz_mode: bool,
    fuzz_seed: u64,
) -> anyhow::Result<(Vec<PrepareInfo>, Vec<SetupFailureInfo>)> {
    let mut handles: Vec<_> = Vec::new();
    let verbose = parsed.verbose;

    let (send, mut recv) = tokio::sync::mpsc::channel(100);
    for (key, matrix) in runner_source_map.into_iter() {
        // for each runner, allocate runner dir
        let runner_dir = binary_cache.runner_dir()?;
        let send = send.clone();
        println!(
            "Running test setup: runner={}, option_set={}, source={}",
            key.runner_name, key.option_set_name, key.source_name
        );
        println!("-----------------------------------------");
        let current_dir = current_dir.clone();

        let source_dir = if let Some(map) = &source_dir_map {
            if let Some(dir) = map.get(&key.source_name) {
                dir.clone()
            } else {
                String::new()
            }
        } else {
            String::new()
        };

        let sema = sema.clone();
        let h: JoinHandle<anyhow::Result<()>> = tokio::spawn(async move {
            if matrix.setup_command.is_empty() {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    format!(
                        "runner.source_setup_command is empty: runner name={}",
                        key.runner_name
                    ),
                )
                .into());
            }
            // replace $WORK_DIR to current working dir
            let replaced_setup_command: Vec<String> = matrix
                .setup_command
                .iter()
                .map(|s| s.replace("$WORK_DIR", &current_dir))
                .collect();

            #[allow(unused_variables)]
            let bound = if let Some(sema) = &sema {
                let permit = match sema.acquire().await {
                    Ok(permit) => permit,
                    Err(e) => {
                        return Err(anyhow::anyhow!("failed to acquire semaphore permit for test setup: runner={}, option_set={}, source={}, error={}", key.runner_name, key.option_set_name, key.source_name, e));
                    }
                };
                Some(permit)
            } else {
                None
            };

            let spawned = tokio::process::Command::new(replaced_setup_command[0].clone())
                .args(&replaced_setup_command[1..])
                .current_dir(&runner_dir)
                .env("UNICTEST_VERBOSE", if verbose { "1" } else { "0" })
                .env("UNICTEST_ORIGINAL_WORK_DIR", &current_dir)
                .env("UNICTEST_SOURCE_FILE", &key.source_name)
                .env("UNICTEST_RUNNER_DIR", &runner_dir)
                .env("UNICTEST_RUNNER_NAME", &key.runner_name)
                .env(
                    "UNICTEST_CANDIDATE_FORMATS",
                    &matrix.candidate_formats.join(","),
                )
                .env("UNICTEST_SOURCE_SETUP_DIR", &source_dir)
                .env("UNICTEST_OPTION_SET_NAME", &key.option_set_name)
                .env(
                    "UNICTEST_OPTION_SET_SETUP_OPTIONS",
                    &matrix.option_set.setup_options.join(","),
                )
                .env("UNICTEST_FUZZ_MODE", if fuzz_mode { "1" } else { "0" })
                .env("UNICTEST_FUZZ_SEED", fuzz_seed.to_string())
                .output()
                .await?;
            send.send((
                spawned,
                PrepareInfo {
                    runner: key.runner_name,
                    option_set: matrix.option_set,
                    source: key.source_name,
                    candidate_formats: matrix.candidate_formats,
                    runner_dir,
                    run_command: matrix.run_command,
                },
            ))
            .await?;
            Ok(())
        });
        handles.push(h);
    }
    drop(send);
    let mut setup_infos = Vec::new();
    let mut setup_failures = Vec::new();
    while let Some((output, prepare_info)) = recv.recv().await {
        println!(
            "Test setup completed: runner name={}, source={}",
            prepare_info.runner, prepare_info.source
        );
        print_output(&output, parsed.print_stdout);
        if !output.status.success() {
            println!("Test setup failed, skipping tests for this setup.");
            setup_failures.push(SetupFailureInfo::CommandFailure {
                source: prepare_info.source,
                runner: prepare_info.runner,
                option_set: prepare_info.option_set.name,
                output_stdout: String::from_utf8_lossy(&output.stdout).into_owned(),
                output_stderr: String::from_utf8_lossy(&output.stderr).into_owned(),
            });
        } else {
            setup_infos.push(prepare_info);
        }
        println!("----------------------------------------");
    }

    let all_errors = future::try_join_all(handles).await?;
    for res in all_errors {
        if let Err(e) = res {
            println!("Error during setup: {}", e);
            setup_failures.push(SetupFailureInfo::OrchestrationError {
                error_message: format!("orchestration error during setup: {}", e),
            });
        }
    }
    Ok((setup_infos, setup_failures))
}

async fn run_tests(
    sema: &Option<std::sync::Arc<tokio::sync::Semaphore>>,
    parsed: &Args,
    setup_infos: &Vec<PrepareInfo>,
    inputs: Vec<TestInput>,
    binary_cache: &mut InputBinaryCache,
    current_dir: &String,
    fuzz_mode: bool,
    fuzz_seed: u64,
) -> anyhow::Result<Vec<TestCaseResult>> {
    let mut test_handles = Vec::new();
    let verbose = parsed.verbose;

    let (send2, mut recv2) = tokio::sync::mpsc::channel(100);
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
            if p.source != input.source || !p.candidate_formats.contains(&input.format_name) {
                continue;
            }
            let input = input.clone();
            let input_path =
                binary_cache.read_input_binary(&PathBuf::from(&input.binary), input.hex)?;
            let runner_command = p.run_command.clone();
            let source = p.source.clone();
            let runner_dir = p.runner_dir.clone();
            let runner = p.runner.clone();
            let send2 = send2.clone();
            let option_set = p.option_set.clone();
            println!(
                "Running test: runner={}, option_set={}, input={} source={}, format={}, binary={} hex={}",
                runner, option_set.name, input.name, source, input.format_name, input.binary, input.hex
            );
            println!("----------------------------------------");
            let current_dir = current_dir.clone();
            let shorter = parsed.shorter_path;
            let sema = sema.clone();
            let h: JoinHandle<()> = tokio::spawn(async move {
                let send3 = send2.clone();
                let result_info = ResultInfo {
                    runner: runner.clone(),
                    source: source.clone(),
                    option_set: option_set.name.clone(),
                    format_name: input.format_name.clone(),
                    input_name: input.name.clone(),
                    output: None,
                    failure_case: input.failure_case,
                };
                let mut result_info2 = result_info.clone();

                let f: anyhow::Result<()> = async move {
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
                    let task_dir = if shorter {
                        let rand_str = InputBinaryCache::get_short_random_string();
                        runner_dir.join(format!("t-{}", rand_str))
                    } else {
                        runner_dir.join(format!(
                            "task-{}-{}-{}",
                            input.name,
                            input.format_name,
                            Uuid::new_v4()
                        ))
                    };
                    fs::create_dir_all(&task_dir)?;
                    let replaced_run_command: Vec<String> = runner_command
                        .iter()
                        .map(|s| s.replace("$WORK_DIR", &current_dir))
                        .collect();

                    #[allow(unused_variables)]
                    let bound= if let Some(sema) = &sema {
                        let permit = match sema.acquire().await {
                            Ok(permit) => permit,
                            Err(e) => {
                                return Err(anyhow::anyhow!("failed to acquire semaphore permit for test execution: runner={}, option_set={}, source={}, format={}, input={}, error={}", runner, option_set.name, source, input.format_name, input.name, e));
                            }
                        };
                        Some(permit)
                    } else {
                        None
                    };

                    let output = tokio::process::Command::new(replaced_run_command[0].clone())
                        .args(&replaced_run_command[1..])
                        .current_dir(&task_dir)
                        .env("UNICTEST_VERBOSE", if verbose { "1" } else { "0" })
                        .env("UNICTEST_ORIGINAL_WORK_DIR", &current_dir)
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
                        .env("UNICTEST_OPTION_SET_NAME", &option_set.name)
                        .env(
                            "UNICTEST_OPTION_SET_SETUP_OPTIONS",
                            &option_set.setup_options.join(","),
                        )
                        .env(
                            "UNICTEST_OPTION_SET_RUN_OPTIONS",
                            &option_set.run_options.join(","),
                        )
                        .env("UNICTEST_FUZZ_MODE", if fuzz_mode { "1" } else { "0" })
                        .env("UNICTEST_FUZZ_SEED", fuzz_seed.to_string())
                        .output()
                        .await?;
                    result_info2.output = Some(output);
                    send2.send(Ok(result_info2)).await?;
                    Ok(())
                }
                .await;
                if let Err(e) = f {
                    if let Err(send_err) = send3.send(Err((e, result_info))).await {
                        eprintln!("Failed to send error result: {}", send_err);
                    }
                }
            });
            test_handles.push(h);
        }
    }
    drop(send2);
    if let Some(expected_count) = parsed.expected_test_total {
        let actual_count = test_handles.len();
        if expected_count != actual_count {
            println!(
                "{}: Expected test count {}, but got {}",
                "FAIL".red(),
                expected_count,
                actual_count
            );
            std::process::exit(1);
        }
    }
    let mut test_results = Vec::new();

    while let Some(result) = recv2.recv().await {
        let result = match result {
            Ok(x) => x,
            Err((e, result)) => {
                println!(
                    "{}: Error during test execution: runner={}, option_set={}, source={}, format={}, input={}: {}",
                    "FAIL".red(),
                    result.runner,
                    result.option_set,
                    result.source,
                    result.format_name,
                    result.input_name,
                    e
                );
                println!("----------------------------------------");
                test_results.push(TestCaseResult {
                    runner: result.runner,
                    option_set: result.option_set,
                    source: result.source,
                    format_name: result.format_name,
                    input_name: result.input_name,
                    success: false,
                    failure_case: result.failure_case,
                    output_stdout: String::new(),
                    output_stderr: String::new(),
                    output_status: 0,
                    status_interpretation: "test execution error".to_string(),
                    error_message: Some(format!("test execution error: {}", e)),
                });
                continue;
            }
        };
        let output = result.output.as_ref().unwrap();
        let is_passed = if fuzz_mode {
            // In fuzz mode: success or decode error are both acceptable
            // (random input may not be valid for the format).
            // Only crashes and encode errors are failures.
            output.status.success()
                || output.status.code() == Some(CONFIRMED_DECODER_FAILURE_EXIT_CODE)
        } else {
            (output.status.success() && !result.failure_case)
                || (output.status.code() == Some(CONFIRMED_DECODER_FAILURE_EXIT_CODE)
                    && result.failure_case)
        };

        println!(
            "{}: Test completed runner name={}, source={}, format={}, input={}",
            if is_passed {
                "PASS".green()
            } else {
                "FAIL".red()
            },
            result.runner,
            result.source,
            result.format_name,
            result.input_name
        );
        print_output(output, parsed.print_stdout);
        let message = if fuzz_mode {
            if output.status.success() {
                "fuzz: roundtrip succeeded"
            } else if output.status.code() == Some(CONFIRMED_DECODER_FAILURE_EXIT_CODE) {
                "fuzz: decode error (acceptable)"
            } else if output.status.code() == Some(UNEXPECTED_ENCODER_FAILURE_EXIT_CODE) {
                "fuzz: unexpected encode error"
            } else {
                "fuzz: crash or internal error"
            }
        } else if output.status.success() && result.failure_case {
            "expected failure, but succeeded"
        } else if output.status.code() == Some(CONFIRMED_DECODER_FAILURE_EXIT_CODE)
            && !result.failure_case
        {
            "expected success, but failed"
        } else if output.status.code() == Some(CONFIRMED_DECODER_FAILURE_EXIT_CODE)
            && result.failure_case
        {
            "decode error as expected"
        } else if output.status.code() == Some(UNEXPECTED_ENCODER_FAILURE_EXIT_CODE) {
            "unexpected encode error"
        } else if !output.status.success() {
            "test internal error"
        } else {
            "test succeeded as expected"
        };
        println!("Test result: {}", message);
        println!("----------------------------------------");
        test_results.push(TestCaseResult {
            runner: result.runner.clone(),
            option_set: result.option_set.clone(),
            source: result.source.clone(),
            format_name: result.format_name.clone(),
            input_name: result.input_name.clone(),
            success: is_passed,
            failure_case: result.failure_case,
            output_stdout: String::from_utf8_lossy(&output.stdout).to_string(),
            output_stderr: String::from_utf8_lossy(&output.stderr).to_string(),
            output_status: output.status.code().unwrap_or(-1),
            status_interpretation: message.to_string(),
            error_message: None,
        });
    }

    futures::future::try_join_all(test_handles).await?;
    Ok(test_results)
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let timer = std::time::Instant::now();
    let parsed = Args::parse();
    let test_config = fs::read_to_string(Path::new(&parsed.test_config_file))?;
    let mut d1 = serde_json::Deserializer::from_str(&test_config);
    let test_config = TestConfig::deserialize(&mut d1)?;
    println!("Loaded test config from file {}", parsed.test_config_file);
    let current_dir = std::env::current_dir()?;
    let current_dir = path_str(&current_dir);
    // replace RunnerConfig::File with actual TestRunner
    let test_runner = test_config
        .runners
        .into_iter()
        .map(|runner| -> Result<TestRunner, anyhow::Error> {
            let mut runner = match runner {
                RunnerConfig::TestRunner(runner) => runner,
                RunnerConfig::File(runner_file) => {
                    // replace $WORK_DIR in file path
                    let runner_file = runner_file.file.replace("$WORK_DIR", &current_dir);
                    let runner_config_str = fs::read_to_string(Path::new(&runner_file))?;
                    let mut d2 = serde_json::Deserializer::from_str(&runner_config_str);
                    let mut actual_runner: TestRunner = TestRunner::deserialize(&mut d2)?;
                    println!("Loaded runner config from file {}", runner_file);
                    actual_runner.file = Some(runner_file.clone());
                    actual_runner
                }
            };
            if runner.option_sets.is_none() {
                runner.option_sets = Some(vec![]);
            }
            if runner.option_sets.as_ref().unwrap().is_empty() {
                // add default empty option set if option_sets is empty
                println!(
                    "Runner {} has no option set, adding default empty option set",
                    runner.name
                );
                runner.option_sets.as_mut().unwrap().push(OptionSet {
                    name: "default".into(),
                    setup_options: vec![],
                    run_options: vec![],
                });
            }
            Ok(runner)
        })
        .filter(|r| {
            if parsed.target_runner.is_empty() {
                true
            } else {
                match r {
                    Ok(runner) => parsed.target_runner.contains(&runner.name),
                    Err(_) => true,
                }
            }
        })
        .collect::<Result<Vec<_>, _>>()?;
    // replace $WORK_DIR in binary and source paths
    let inputs = match test_config.inputs {
        InputConfig::TestInput(input) => input,
        InputConfig::File(file) => {
            // replace $WORK_DIR in file path
            let input_file = file.file.replace("$WORK_DIR", &current_dir);
            let input_config_str = fs::read_to_string(Path::new(&input_file))?;
            let mut d2 = serde_json::Deserializer::from_str(&input_config_str);
            let actual_input: Vec<TestInput> = Vec::<TestInput>::deserialize(&mut d2)?;
            println!("Loaded input config from file {}", input_file);
            actual_input
        }
    };
    let inputs = inputs
        .into_iter()
        .filter(|input| {
            if parsed.target_input.is_empty() {
                true
            } else {
                parsed.target_input.contains(&input.name)
            }
        })
        .map(|mut input| {
            input.binary = input.binary.replace("$WORK_DIR", &current_dir);
            input.source = input.source.replace("$WORK_DIR", &current_dir);
            input
        })
        .collect::<Vec<_>>();
    let mut binary_cache = InputBinaryCache::new(parsed.shorter_path);

    let (mut runner_source_map, source_format_map) =
        make_runner_input_matrix(&test_runner, &inputs);
    let scheduling_count = runner_source_map.len();

    let mut source_dir_map = None;

    let mut setup_failures = Vec::new();

    let sema = if let Some(mut parallel_setup) = parsed.parallel_limit {
        if parallel_setup == 0 {
            parallel_setup = thread::available_parallelism()
                .map(|n| n.get())
                .unwrap_or(4);
        }
        println!("Running setups with parallel limit: {}", parallel_setup);
        Some(std::sync::Arc::new(tokio::sync::Semaphore::new(
            parallel_setup,
        )))
    } else {
        None
    };

    if let Some(common_setup_commands) = &test_config.common_source_setup {
        let (fail_count, source_dir_map_) = run_source_setup(
            &sema,
            common_setup_commands.clone(),
            source_format_map,
            &mut runner_source_map,
            &mut binary_cache,
            &current_dir,
            &parsed,
        )
        .await?;
        setup_failures.extend(fail_count);
        source_dir_map = Some(source_dir_map_);
        println!("Common source setup completed in {:?}", timer.elapsed());
        println!("----------------------------------------");
    }

    let fuzz_mode = parsed.fuzz;
    let fuzz_seed = parsed.fuzz_seed.unwrap_or(0);

    let (setup_infos, fail_count2) = run_setup(
        &sema,
        runner_source_map,
        source_dir_map,
        &mut binary_cache,
        &current_dir,
        &parsed,
        fuzz_mode,
        fuzz_seed,
    )
    .await?;
    setup_failures.extend(fail_count2);

    println!("Setup all tests in {:?}", timer.elapsed());
    println!("----------------------------------------");

    let test_results = run_tests(
        &sema,
        &parsed,
        &setup_infos,
        inputs,
        &mut binary_cache,
        &current_dir,
        fuzz_mode,
        fuzz_seed,
    )
    .await?;

    let failed_count = test_results.iter().filter(|&x| !x.success).count();
    let has_scheduling_failures = !setup_failures.is_empty();
    println!("All tests completed in {:?}", timer.elapsed());
    println!(
        "Total: {}, {}: {}, {}: {}",
        test_results.len(),
        "PASS".green(),
        test_results.len() - failed_count,
        "FAIL".red(),
        failed_count
    );
    if !setup_failures.is_empty() {
        println!(
            "{}: {} source file setups failed due to setup errors (total {} source files)",
            "FAIL".red(),
            setup_failures.len(),
            scheduling_count
        );
    }
    if let Some(output_json) = &parsed.output_json {
        let test_results_summary = TestResults {
            success_count: test_results.len() - failed_count,
            fail_count: failed_count,
            setup_fail_count: setup_failures.len(),
            setup_failures,
            results: test_results,
        };
        fs::write(
            output_json,
            serde_json::to_string_pretty(&test_results_summary)?,
        )?;
        println!("Test results saved to {}", output_json);
    }

    if parsed.save_tmp_dir {
        binary_cache.print_tmp_dir();
    } else {
        binary_cache.remove_tmp_dir();
    }
    if parsed.clean_tmp {
        let tmp_dir = std::env::temp_dir().join("unictest");
        if tmp_dir.exists() {
            if parsed.save_tmp_dir {
                let current_tmp = binary_cache.tmp_dir.clone();
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
    if failed_count > 0 || has_scheduling_failures {
        std::process::exit(1);
    }
    Ok(())
}
