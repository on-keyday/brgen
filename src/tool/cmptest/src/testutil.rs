use std::{collections::HashMap, env, fs, path::PathBuf, sync::Arc};

use rand::{self, distr::SampleString};

use serde::{Deserialize, Serialize};
use tokio::sync::mpsc;

#[derive(Serialize, Deserialize, Debug)]

pub struct GeneratedFileInfo {
    pub dir: String,

    pub base: String,

    pub suffix: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct TestInfo {
    pub total_count: u64,

    pub error_count: u64,

    pub time: String,

    pub generated_files: Vec<GeneratedFileInfo>,
}

impl GeneratedFileInfo {
    pub fn into_path(&self) -> String {
        format!("{}/{}{}", self.dir, self.base, self.suffix)
    }
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct TestRunner {
    // file suffix of generated files
    pub suffix: String,
    // test template file
    pub test_template: String,
    // replace target of struct in test_template file
    pub replace_struct_name: String,
    // replace target of file name of test target file
    pub replace_file_name: String,

    pub build_input_name: String,
    pub build_output_name: String,
    // command to build test
    // $INPUT is replaced with test file path
    // $OUTPUT is replaced with output file path
    pub build_command: Vec<String>,
    // command to run test
    // $INPUT is replaced with test input file path
    // $EXEC is replaced with test exec file path that is built by build_command
    // $OUTPUT is replaced with test output file path
    pub run_command: Vec<String>,

    #[serde(skip_deserializing)]
    pub file: Option<String>,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct TestInput {
    // input binary file
    pub binary: String,
    // test target format name
    pub format_name: String,
    // file base name that contains format_name format
    pub file_base: String,
    // this input is failure case
    pub failure_case: bool,
    // binary is actually hex string file
    pub hex: bool,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct TestRunnerFile {
    pub file: String,
}

#[derive(Serialize, Deserialize, Clone, Debug)]
#[serde(untagged)]
pub enum RunnerConfig {
    TestRunner(TestRunner),
    File(TestRunnerFile),
}

#[derive(Serialize, Deserialize, Clone)]
pub struct TestConfig {
    pub runners: Vec<RunnerConfig>,
    // test input binary file
    pub inputs: Vec<TestInput>,
}

#[derive(Debug, Clone)]
pub struct TestSchedule {
    pub input: Arc<TestInput>,
    pub runner: Arc<TestRunner>,
    pub file: Arc<GeneratedFileInfo>,
}

impl TestSchedule {
    pub fn test_name(&self) -> String {
        let res = self.file.into_path() + "/" + &self.input.format_name;
        let res = res + " input:" + &self.input.binary;
        let res = if self.input.hex {
            res + "(hex file)"
        } else {
            res
        };
        let res = if self.input.failure_case {
            res + "(require:failure)"
        } else {
            res + "(require:success)"
        };
        res
    }
}

#[derive(Debug)]
pub enum Error {
    IO(std::io::Error),
    Join(tokio::task::JoinError),
    JSON(serde_json::Error),
    Exec(String),
    TestFail(String),
}

impl From<std::io::Error> for Error {
    fn from(s: std::io::Error) -> Error {
        Error::IO(s)
    }
}

impl From<tokio::task::JoinError> for Error {
    fn from(s: tokio::task::JoinError) -> Error {
        Error::Join(s)
    }
}

impl From<serde_json::Error> for Error {
    fn from(s: serde_json::Error) -> Error {
        Error::JSON(s)
    }
}

pub struct TestScheduler {
    template_files: HashMap<String, String>,
    tmpdir: Option<PathBuf>,
    input_binaries: HashMap<(PathBuf, bool), (PathBuf, Vec<u8>)>,
    debug: bool,
}

fn path_str(path: &PathBuf) -> String {
    match path.to_str() {
        Some(x) => x.to_string(),
        None => path.to_string_lossy().to_string(),
    }
}

type SendChan =
    mpsc::Sender<Result<(TestSchedule, String), (TestSchedule, String, Error, Vec<String>)>>;

impl TestScheduler {
    pub fn new(debug: bool) -> Self {
        Self {
            template_files: HashMap::new(),
            tmpdir: None,
            input_binaries: HashMap::new(),
            debug: debug,
        }
    }

    fn read_text_file(&mut self, path: &str) -> Result<String, Error> {
        if let Some(x) = self.template_files.get(path) {
            return Ok(x.clone());
        } else {
            let t = match fs::read_to_string(path) {
                Ok(x) => x,
                Err(x) => {
                    return Err(std::io::Error::new(
                        std::io::ErrorKind::Other,
                        format!("read file error: {path}: {x}"),
                    )
                    .into());
                }
            };
            self.template_files.insert(path.to_string(), t);
            Ok(self.template_files.get(path).unwrap().clone())
        }
    }

    fn compile_hex(input: Vec<u8>) -> Result<Vec<u8>, Error> {
        // skip space,tab,line and # line comment
        let mut hex = Vec::new();
        let mut i = 0;
        let mut pair: Option<u8> = None;
        let mut line = 1;
        let mut col = 1;
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

    fn read_input_binary(
        &mut self,
        path: &PathBuf,
        is_hex: bool,
    ) -> Result<(PathBuf, Vec<u8>), Error> {
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
                let c = Self::compile_hex(t)?;
                let tmp_path = self.get_tmp_dir();
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

    fn gen_random() -> String {
        rand::distr::Alphanumeric.sample_string(&mut rand::rng(), 32)
    }

    fn get_tmp_dir<'a>(&'a mut self) -> PathBuf {
        if let Some(x) = self.tmpdir.as_ref() {
            x.clone()
        } else {
            let dir = env::temp_dir();
            let dir = dir.join(String::from("cmptest-") + &Self::gen_random());
            self.tmpdir = Some(dir);
            self.tmpdir.as_ref().unwrap().clone()
        }
    }

    fn prepare_content(&mut self, sched: &TestSchedule) -> Result<String, Error> {
        // get template and replace with target
        let template = self.read_text_file(&sched.runner.test_template)?;
        let replace_with = &sched.runner.replace_struct_name;
        let template = template.replace(replace_with, &sched.input.format_name);
        let replace_with = &sched.runner.replace_file_name;
        let path = format!("{}/{}", sched.file.dir, sched.file.base);
        let instance = template.replace(replace_with, &path);
        Ok(instance)
    }

    fn create_test_dir(&mut self, sched: &TestSchedule) -> Result<PathBuf, Error> {
        let tmp_dir = self.get_tmp_dir();
        let tmp_dir = tmp_dir.join(&sched.file.base);
        let tmp_dir = tmp_dir.join(&sched.input.format_name);
        let tmp_dir = tmp_dir.join(Self::gen_random());
        fs::create_dir_all(&tmp_dir)?;
        Ok(tmp_dir)
    }

    fn create_input_file(
        &mut self,
        sched: &TestSchedule,
        tmp_dir: &PathBuf,
        instance: String,
    ) -> Result<(PathBuf, PathBuf), Error> {
        let input_file = tmp_dir.join(&sched.runner.build_input_name);
        let output_file = tmp_dir.join(&sched.runner.build_output_name);
        fs::write(&input_file, instance)?;
        Ok((input_file, output_file))
    }

    fn replace_cmd(
        cmd: &mut Vec<String>,
        sched: &TestSchedule,
        tmp_dir: &PathBuf,
        input: &PathBuf,
        output: &PathBuf,
        exec: Option<&PathBuf>,
        debug: bool,
    ) {
        for c in cmd {
            if c == "$INPUT" {
                *c = input.to_str().unwrap().to_string();
            }
            if c == "$OUTPUT" {
                *c = output.to_str().unwrap().to_string();
            }
            if c == "$EXEC" {
                if let Some(e) = exec {
                    *c = e.to_str().unwrap().to_string();
                }
            }
            if c == "$TMPDIR" {
                *c = tmp_dir.to_str().unwrap().to_string();
            }
            if c == "$ORIGIN" {
                *c = format!("{}/{}", sched.file.dir, sched.file.base);
            }
            if c == "$CONFIG" {
                if let Some(x) = sched.runner.file.as_ref() {
                    *c = x.clone();
                }
            }
            if c == "$DEBUG" {
                *c = if debug {
                    String::from("true")
                } else {
                    String::from("false")
                };
            }
        }
    }

    async fn exec_cmd(
        sched: &TestSchedule,
        base: &Vec<String>,
        tmp_dir: &PathBuf,
        input: &PathBuf,
        output: &PathBuf,
        exec: Option<&PathBuf>,
        expect_ok: bool,
        debug: bool,
    ) -> (
        Result<Option<String>, Error>,
        String,      // stdout
        Vec<String>, /*command line*/
    ) {
        let mut cmd = base.clone();
        Self::replace_cmd(&mut cmd, sched, tmp_dir, input, output, exec, debug);
        let mut r = tokio::process::Command::new(&cmd[0]);
        r.args(&cmd[1..]);
        let done = match r.output().await {
            Ok(x) => x,
            Err(x) => {
                return (
                    Err(Error::Exec(format!("exec error: {:?}: {}", cmd, x))),
                    String::new(),
                    cmd,
                );
            }
        };
        let code = done.status.code();
        let stdout = String::from_utf8_lossy(&done.stdout);
        match code {
            Some(0) => return (Ok(None), stdout.to_string(), cmd),
            status => {
                if let Some(x) = status {
                    if x == 1 && !expect_ok {
                        return (
                            Ok(Some(format!(
                                "process exit with 1:\n{}",
                                String::from_utf8_lossy(&done.stderr)
                            ))),
                            stdout.to_string(),
                            cmd,
                        );
                    }
                }
                return (
                    Err(Error::Exec(format!(
                        "process exit with {:?}:\n{}",
                        status,
                        String::from_utf8_lossy(&done.stderr)
                    ))),
                    stdout.to_string(),
                    cmd,
                );
            }
        }
    }

    pub fn run_test_schedule_impl(
        sched: TestSchedule,
        send: SendChan,
        tmp_dir: PathBuf,
        input: PathBuf,
        input_path: PathBuf,
        output: PathBuf,
        input_binary: Vec<u8>,
        debug: bool,
    ) -> Result<tokio::task::JoinHandle<()>, Error> {
        let proc = async move {
            let mut stdout_buf = String::new();
            // build test
            match Self::exec_cmd(
                &sched,
                &sched.runner.build_command,
                &tmp_dir,
                &input,
                &output,
                None,
                true,
                debug,
            )
            .await
            {
                (Ok(_), stdout, _) => {
                    stdout_buf = stdout;
                }
                (Err(x), stdout, cmd) => return Err((sched, stdout, x, cmd)),
            };

            let exec = output;

            let output = tmp_dir.join("output.bin");

            // run test
            let (status, cmdline) = {
                let (status, stdout, cmdline) = Self::exec_cmd(
                    &sched,
                    &sched.runner.run_command,
                    &tmp_dir,
                    &input_path,
                    &output,
                    Some(&exec),
                    false,
                    debug,
                )
                .await;
                stdout_buf += &stdout;
                match status {
                    Ok(x) => (x, cmdline),
                    Err(x) => return Err((sched, stdout_buf, x, cmdline)),
                }
            };

            let expect = !sched.input.failure_case;
            let actual = status.is_none();

            if actual != expect {
                let actual = if actual { "success" } else { "failure" };
                let expect = if expect { "success" } else { "failure" };
                if actual == "success" {
                    return Err((
                        sched,
                        stdout_buf,
                        Error::TestFail(format!(
                            "test failed: expect {} but got {}",
                            expect, actual
                        )),
                        cmdline,
                    ));
                } else {
                    return Err((
                        sched,
                        stdout_buf,
                        Error::TestFail(format!(
                            "test failed: expect {} but got {}: {}",
                            expect,
                            actual,
                            status.unwrap()
                        )),
                        cmdline,
                    ));
                }
            }

            if sched.input.failure_case {
                return Ok((sched, stdout_buf)); // skip output check
            }

            let output = match tokio::fs::read(&output).await {
                Ok(x) => x,
                Err(x) => {
                    return Err((
                        sched,
                        stdout_buf,
                        Error::TestFail(format!("test output file cannot load: {}", x)),
                        Vec::new(),
                    ))
                }
            };

            let min_size = if input_binary.len() < output.len() {
                input_binary.len()
            } else {
                output.len()
            };

            let mut diff = Vec::new();

            for i in 0..min_size {
                if input_binary[i] != output[i] {
                    diff.push((i, Some(input_binary[i]), Some(output[i])));
                }
            }

            if input_binary.len() != output.len() {
                if input_binary.len() > output.len() {
                    for i in output.len()..input_binary.len() {
                        diff.push((i, Some(input_binary[i]), None));
                    }
                } else {
                    for i in input_binary.len()..output.len() {
                        diff.push((i, None, Some(output[i])))
                    }
                }
            }

            if !diff.is_empty() {
                let mut debug = format!("input and output is different\n");
                for (i, a, b) in diff {
                    debug += &format!("{}: {:02x?} != {:02x?}\n", i, a, b);
                }
                return Err((sched, stdout_buf, Error::TestFail(debug), cmdline));
            }

            Ok((sched, stdout_buf))
        };
        let proc = async move {
            let r = proc.await;
            send.send(r).await.unwrap();
        };
        Ok(tokio::spawn(proc))
    }

    pub fn run_test_schedule(
        &mut self,
        sched: &TestSchedule,
        send: SendChan,
    ) -> Result<tokio::task::JoinHandle<()>, Error> {
        let tmp_dir = self.create_test_dir(sched)?;
        let instance = self.prepare_content(sched)?;

        let (input, output) = self.create_input_file(sched, &tmp_dir, instance)?;
        let input_path: PathBuf = sched.input.binary.clone().into();
        let (input_path, input_binary) = self.read_input_binary(&input_path, sched.input.hex)?;

        Self::run_test_schedule_impl(
            sched.clone(),
            send,
            tmp_dir,
            input,
            input_path,
            output,
            input_binary,
            self.debug,
        )
    }

    pub fn remove_tmp_dir(self) {
        if let Some(dir) = self.tmpdir {
            fs::remove_dir_all(dir).unwrap();
        }
    }

    pub fn print_tmp_dir(self) -> Option<PathBuf> {
        if let Some(dir) = self.tmpdir {
            println!("tmp directory is {}", path_str(&dir));
            Some(dir)
        } else {
            None
        }
    }
}
