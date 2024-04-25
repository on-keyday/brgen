use std::{
    collections::HashMap,
    env,
    ffi::OsStr,
    fs, os,
    path::{Path, PathBuf},
    process,
};

use clap::{arg, Parser};
use serde::Deserialize;
use testutil::TestSchedule;
mod testutil;
use rand::{
    self,
    distributions::{Alphanumeric, DistString},
};

#[derive(Parser)]
struct Args {
    #[arg(long, short('f'))]
    test_info_file: String,
    #[arg(long, short('c'))]
    test_config_file: String,
}
type Error = Box<dyn std::error::Error>;

struct TestScheduler {
    template_files: HashMap<String, String>,
    tmpdir: Option<PathBuf>,
    input_binaries: HashMap<String, Vec<u8>>,
}

impl TestScheduler {
    fn new() -> Self {
        Self {
            template_files: HashMap::new(),
            tmpdir: None,
            input_binaries: HashMap::new(),
        }
    }

    fn read_template(&mut self, path: &str) -> Result<String, Error> {
        if let Some(x) = self.template_files.get(path) {
            return Ok(x.clone());
        } else {
            let t = fs::read_to_string(path)?;
            self.template_files.insert(path.to_string(), t);
            Ok(self.template_files.get(path).unwrap().clone())
        }
    }

    fn read_input_binary(&mut self, path: &str) -> Result<Vec<u8>, Error> {
        if let Some(x) = self.input_binaries.get(path) {
            return Ok(x.clone());
        } else {
            let t = fs::read(path)?;
            self.input_binaries.insert(path.to_string(), t);
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
        let template = self.read_template(&sched.runner.test_template)?;
        let replace_with = &sched.runner.replace_struct_name;
        let template = template.replace(replace_with, &sched.input.format_name);
        let replace_with = &sched.runner.replace_file_name;
        let path = format!("{}/{}", sched.file.dir, sched.file.base);
        let instance = template.replace(replace_with, &path);
        Ok(instance)
    }

    fn create_input_file<'a>(
        &mut self,
        sched: &TestSchedule<'a>,
        instance: String,
    ) -> Result<(PathBuf, PathBuf, PathBuf), Error> {
        let tmp_dir = self.get_tmp_dir();
        let tmp_dir = tmp_dir.join(&sched.file.base);
        let tmp_dir = tmp_dir.join(&sched.input.format_name);
        let tmp_dir = tmp_dir.join(&sched.file.suffix);
        fs::create_dir_all(&tmp_dir)?;
        let input_file = tmp_dir.join(&sched.runner.build_input_name);
        let output_file = tmp_dir.join(&sched.runner.build_output_name);
        fs::write(&input_file, instance)?;
        Ok((tmp_dir, input_file, output_file))
    }

    fn replace_cmd(
        cmd: &mut Vec<String>,
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
        }
    }

    fn exec_cmd<'a>(
        &mut self,
        base: &Vec<String>,
        tmp_dir: &PathBuf,
        input: &PathBuf,
        output: &PathBuf,
        exec: Option<&PathBuf>,
        expect_ok: bool,
    ) -> Result<bool, Error> {
        let mut cmd = base.clone();
        Self::replace_cmd(&mut cmd, tmp_dir, input, output, exec);
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
        let instance = self.prepare_content(sched)?;

        let (tmp_dir, input, output) = self.create_input_file(sched, instance)?;

        // build test
        self.exec_cmd(
            &sched.runner.build_command,
            &tmp_dir,
            &input,
            &output,
            None,
            true,
        )?;

        let exec = output;

        let output = tmp_dir.join("output.bin");

        let input_binary = sched.input.binary.clone().into();

        // run test
        let status = self.exec_cmd(
            &sched.runner.run_command,
            &tmp_dir,
            &input_binary,
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

        let input_binary = self.read_input_binary(&input_binary.to_string_lossy())?; // check input is valid
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
            eprintln!("test failed: input and output is different");
            for (i, a, b) in diff {
                eprintln!("{}: {:02x?} != {:02x?}", i, a, b);
            }
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "test failed: input and output is different",
            )
            .into());
        }

        Ok(())
    }
}

fn main() -> Result<(), Error> {
    let parsed = Args::parse();
    let test_config = fs::read_to_string(Path::new(&parsed.test_config_file))?;
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
        has_exec = false;
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
        let ext = file.base.split('.').last().unwrap();
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
            if let Some(input) = file_format_list.get(&(file.base.clone(), s.clone())) {
                sched.push(TestSchedule {
                    runner: runner,
                    input: input,
                    file: file,
                })
            }
        }
    }
    let mut scheduler = TestScheduler::new();
    for s in sched {
        scheduler.run_test_schedule(&s)?;
    }
    Ok(())
}
