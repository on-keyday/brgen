use std::path::{Path, PathBuf};

use ast2rust::ast;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize)]

pub struct LineMap {
    pub line: u64,

    pub loc: ast::Loc,
}

#[derive(Serialize, Deserialize)]

pub struct GeneratedData {
    pub structs: Vec<String>,

    pub line_map: Vec<LineMap>,
}

#[derive(Serialize, Deserialize)]

pub struct GeneratedFileInfo {
    pub dir: String,

    pub base: String,

    pub suffix: String,
}

#[derive(Serialize, Deserialize)]
pub struct TestInfo {
    pub total_count: u64,

    pub err_count: u64,

    pub time: String,

    pub generated_files: Vec<GeneratedFileInfo>,
}

impl GeneratedFileInfo {
    pub fn into_path(&self) -> String {
        format!("{}/{}.{}", self.dir, self.base, self.suffix)
    }
}

#[derive(Serialize, Deserialize, Clone)]
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

#[derive(Serialize, Deserialize, Clone)]
pub struct TestInput {
    // input binary file
    pub binary: String,
    // test target format name
    pub format_name: String,
    // file base name that contains format_name format
    pub file_base: String,
    // this input is failure case
    pub failure_case: bool,
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
