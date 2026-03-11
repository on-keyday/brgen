# Development Cycle

Action order below is NO MATTER. You can choose actions below as if you need

## 0. Important Constraints

- **Do not edit `src/old/bm2{lang_name}/*.[h|c]pp` directly**: Avoid manual edits in these files because they are auto generated file. Before you modify, ensure following this rule.
- **Read and Update `docs/cline`**: Before you act, you have to write previous `docs/cline`. write your plan and action each time when you found the important things to note at `docs/cline` directory.

## 1. Setup and Configuration

- **Edit `bm2{lang_name}/config.json`**: Modify language-specific placeholders. Base templates are generated using `tool\gen_template --mode config`.
- **Add new config to `config.json`**: Modify `Flags` in `bm2/gen_template/flags.hpp` and add the config name to `MAP_TO_MACRO`.
- **Edit `LANG_LIST` in `script/generate.py`**: Add new languages by updating the language list and running `script/generate`.

## 2. Template and Hook Management

- **Modify `src/old/bm2/gen_template/`**: For language-independent logic, you’ll need to modify these templates.
- **Modify or add files in `src/old/bm2{lang_name}/hook/`**: For language-specific custom logic, edit these files based on the hook definitions. hook file is piece of C++ code that is not complete by itself. (some exception exists like flags.txt, cmptest_build.txt,cmptest_run.txt)
- **Check hook definitions**: Use `tool\gen_template --print-hooks` to ensure the desired hook file exists before creating it.
- **Read documentation**: `docs/template_parameters.md` for understanding placeholder usage in hooks and configuration files. Regenerate the documentation with `tool\gen_template --mode docs-markdown`.

## 3. Code Generation

- **Generate code**: Use `script/generate` to generate code generators and also specify a DSL file with `script/generate {brgen DSL file}`.
- **Run generated generator**: Use `script/run_generated` to execute generated code and save results in `save/{lang_name}/save.{language_specific_suffix}`.

## 4. Testing

The goal of test is to `PASS` the `script/run_cmptest`

- **Add a test case**: Add a test case file in `src/test/{test_name}.bgn`.
- **Run test case**: Execute the generated test case using `script/generate src/test/{test_name}.bgn`.
- **Run generated generator test**: Use `script/run_cmptest` to execute unit tests for generated generator. you can change input by modifying `testkit/inputs.json`. You MUST NOT write to `setup.py` directly. write python code of `setup.py` into `hook/cmptest_build.txt` instead

## 5. Debugging and Maintenance

- **Ignore `Unimplemented` in `src/old/bm2{lang_name}/bm2{lang_name}.cpp`**: Don't worry about `Unimplemented` unless it causes issues in `save/{lang_name}/save.{language_specific_suffix}`.
- **Locate hook mappings**: Use the comments `// load hook: <hook name>` and `// end hook: <hook name>` in `bm2{lang_name}/*.cpp` to map hook names to hook files.

## 6. Execution Environment

- **Use PowerShell or Bash**: Depending on the OS (Windows or Linux), use PowerShell or Bash for script execution.
