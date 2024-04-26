use std::{collections::HashMap, env, fs, path::PathBuf, process};

use rand::{
    self,
    distributions::{Alphanumeric, DistString},
};

use ast2rust::ast;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]

pub struct LineMap {
    pub line: u64,

    pub loc: ast::Loc,
}

#[derive(Serialize, Deserialize, Debug)]

pub struct GeneratedData {
    pub structs: Vec<String>,

    pub line_map: Vec<LineMap>,
}

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

#[derive(Serialize, Deserialize, Clone)]
pub struct TestConfig {
    pub runners: Vec<TestRunner>,
    // test input binary file
    pub inputs: Vec<TestInput>,
}

pub struct TestSchedule<'a> {
    pub input: &'a TestInput,
    pub runner: &'a TestRunner,
    pub file: &'a GeneratedFileInfo,
}

pub type Error = Box<dyn std::error::Error>;

pub struct TestScheduler {
    template_files: HashMap<String, String>,
    tmpdir: Option<PathBuf>,
    input_binaries: HashMap<PathBuf, Vec<u8>>,
}

impl TestScheduler {
    pub fn new() -> Self {
        Self {
            template_files: HashMap::new(),
            tmpdir: None,
            input_binaries: HashMap::new(),
        }
    }

    fn read_text_file(&mut self, path: &str) -> Result<String, Error> {
        if let Some(x) = self.template_files.get(path) {
            return Ok(x.clone());
        } else {
            let t = fs::read_to_string(path)?;
            self.template_files.insert(path.to_string(), t);
            Ok(self.template_files.get(path).unwrap().clone())
        }
    }

    fn compile_hex(input: Vec<u8>) -> Result<Vec<u8>, Error> {
        // skip space,tab,line and # line comment
        let mut hex = Vec::new();
        let mut i = 0;
        let mut pair: Option<u8> = None;
        while i < input.len() {
            let c = input[i];
            if c == b' ' || c == b'\t' || c == b'\n' {
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
                    format!("invalid hex string at {}:{}", i, c),
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
        }
        if let Some(_) = pair {
            Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "invalid hex string; missing pair",
            )
            .into())
        } else {
            Ok(hex)
        }
    }

    fn read_input_binary(&mut self, path: &PathBuf, is_hex: bool) -> Result<Vec<u8>, Error> {
        if let Some(x) = self.input_binaries.get(path) {
            return Ok(x.clone());
        } else {
            let t = fs::read(path)?;
            let t = if is_hex { Self::compile_hex(t)? } else { t };
            self.input_binaries.insert(path.clone(), t);
            Ok(self.input_binaries.get(path).unwrap().clone())
        }
    }

    fn get_tmp_dir<'a>(&'a mut self) -> PathBuf {
        if let Some(x) = self.tmpdir.as_ref() {
            x.clone()
        } else {
            let dir = env::temp_dir();
            let mut rng = rand::thread_rng();
            let random_str = Alphanumeric.sample_string(&mut rng, 32);
            let dir = dir.join(random_str);
            self.tmpdir = Some(dir);
            self.tmpdir.as_ref().unwrap().clone()
        }
    }

    fn prepare_content<'a>(&mut self, sched: &TestSchedule<'a>) -> Result<String, Error> {
        // get template and replace with target
        let template = self.read_text_file(&sched.runner.test_template)?;
        let replace_with = &sched.runner.replace_struct_name;
        let template = template.replace(replace_with, &sched.input.format_name);
        let replace_with = &sched.runner.replace_file_name;
        let path = format!("{}/{}", sched.file.dir, sched.file.base);
        let instance = template.replace(replace_with, &path);
        Ok(instance)
    }

    fn create_test_dir<'a>(&mut self, sched: &TestSchedule<'a>) -> Result<PathBuf, Error> {
        let tmp_dir = self.get_tmp_dir();
        let tmp_dir = tmp_dir.join(&sched.file.base);
        let tmp_dir = tmp_dir.join(&sched.input.format_name);
        let tmp_dir = tmp_dir.join(&sched.file.suffix);
        fs::create_dir_all(&tmp_dir)?;
        Ok(tmp_dir)
    }

    fn create_input_file<'a>(
        &mut self,
        sched: &TestSchedule<'a>,
        tmp_dir: &PathBuf,
        instance: String,
    ) -> Result<(PathBuf, PathBuf), Error> {
        let input_file = tmp_dir.join(&sched.runner.build_input_name);
        let output_file = tmp_dir.join(&sched.runner.build_output_name);
        fs::write(&input_file, instance)?;
        Ok((input_file, output_file))
    }

    fn replace_cmd<'a>(
        cmd: &mut Vec<String>,
        sched: &TestSchedule<'a>,
        tmp_dir: &PathBuf,
        input: &PathBuf,
        output: &PathBuf,
        exec: Option<&PathBuf>,
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
        }
    }

    fn exec_cmd<'a>(
        &mut self,
        sched: &TestSchedule<'a>,
        base: &Vec<String>,
        tmp_dir: &PathBuf,
        input: &PathBuf,
        output: &PathBuf,
        exec: Option<&PathBuf>,
        expect_ok: bool,
    ) -> Result<bool, Error> {
        let mut cmd = base.clone();
        Self::replace_cmd(&mut cmd, sched, tmp_dir, input, output, exec);
        let mut r = process::Command::new(&cmd[0]);
        r.args(&cmd[1..]);
        let done = r.output()?;
        let code = done.status.code();
        match code {
            Some(0) => return Ok(true),
            status => {
                if let Some(x) = status {
                    if x == 1 && !expect_ok {
                        return Ok(false);
                    }
                }
                let stderr_str = String::from_utf8_lossy(&done.stderr);
                return Err(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    format!("process exit with {:?}\n{}", status, stderr_str),
                )
                .into());
            }
        };
    }

    pub fn run_test_schedule<'a>(&mut self, sched: &TestSchedule<'a>) -> Result<(), Error> {
        let tmp_dir = self.create_test_dir(sched)?;
        let instance = self.prepare_content(sched)?;

        let (input, output) = self.create_input_file(sched, &tmp_dir, instance)?;

        // build test
        self.exec_cmd(
            &sched,
            &sched.runner.build_command,
            &tmp_dir,
            &input,
            &output,
            None,
            true,
        )?;

        let exec = output;

        let output = tmp_dir.join("output.bin");

        let input_binary_name: PathBuf = sched.input.binary.clone().into();

        let input_binary = self.read_input_binary(&input_binary_name, sched.input.hex)?; // check input is valid

        // run test
        let status = self.exec_cmd(
            &sched,
            &sched.runner.run_command,
            &tmp_dir,
            &input_binary_name,
            &output,
            Some(&exec),
            false,
        )?;

        let expect = !sched.input.failure_case;

        if status != expect {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("test failed: expect {} but got {}", expect, status),
            )
            .into());
        }

        let output = fs::read(&output)?; // check output is valid

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
                diff.push((output.len(), None, Some(output[output.len()])));
            } else {
                diff.push((
                    input_binary.len(),
                    Some(input_binary[input_binary.len()]),
                    None,
                ));
            }
        }

        if !diff.is_empty() {
            let mut debug = format!("input and output is different\n");
            for (i, a, b) in diff {
                debug += &format!("{}: {:02x?} != {:02x?}\n", i, a, b);
            }
            return Err(std::io::Error::new(std::io::ErrorKind::Other, debug).into());
        }

        Ok(())
    }
}
