use std::{
    collections::HashMap,
    env,
    ffi::OsStr,
    fs, os,
    path::{Path, PathBuf},
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
}

impl TestScheduler {
    fn read_template(&mut self, path: &str) -> Result<String, Error> {
        if let Some(x) = self.template_files.get(path) {
            return Ok(x.clone());
        } else {
            let t = fs::read_to_string(path)?;
            self.template_files.insert(path.to_string(), t);
            Ok(self.template_files.get(path).unwrap().clone())
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
    ) -> Result<(PathBuf, PathBuf), Error> {
        let tmp_dir = self.get_tmp_dir();
        let tmp_dir = tmp_dir.join(&sched.file.base);
        let tmp_dir = tmp_dir.join(&sched.input.format_name);
        let tmp_dir = tmp_dir.join(&sched.file.suffix);
        fs::create_dir_all(&tmp_dir)?;
        let input_file = tmp_dir.join(&sched.runner.build_input_name);
        let output_file = tmp_dir.join(&sched.runner.build_output_name);
        fs::write(input_file, instance)?;
        Ok((tmp_dir,  output_file))
    }

    pub fn run_test_schedule<'a>(&mut self, sched: &TestSchedule<'a>) -> Result<(), Error> {
        let instance = self.prepare_content(sched)?;

        let (tmp_dir, output) = self.create_input_file(sched, instance)?;

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
    Ok(())
}
