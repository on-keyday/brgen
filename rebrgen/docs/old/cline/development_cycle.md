# Development Cycle (More Concrete)

This is a more concrete version of the development cycle described in `.clinerules/overview.md`. The action order below is still flexible, but this provides more specific steps and examples.

## 0. Important Constraints

- **Do not edit `src/old/bm2{lang_name}/*.[h|c]pp` directly**: Avoid manual edits in these files because they are auto generated file. Before you modify, ensure following this rule.
- **Read and Update `docs/cline`**: Before you act, you have to write previous `docs/cline`. write your plan and action each time when you found the important things to note at `docs/cline` directory.

## 1. Setup and Configuration (Adding a New Language - Example: bm2newlang)

1.  **Edit `bm2{lang_name}/config.json`**: Modify language-specific placeholders. Base templates are generated using `tool\gen_template --mode config`.
    - Example: Create `src/old/bm2newlang/config.json` and add language-specific configurations like file extensions, comment styles, etc.
2.  **Add new config to `config.json`**: Modify `Flags` in `bm2/gen_template/flags.hpp` and add the config name to `MAP_TO_MACRO`.
    - Example: Add a new flag for `newlang` in `src/old/bm2/gen_template/flags.hpp` and update `MAP_TO_MACRO` to include it.
3.  **Edit `LANG_LIST` in `script/generate.py`**: Add new languages by updating the language list and running `script/generate`.
    - Example: Add `"newlang"` to the `LANG_LIST` in `script/generate.py`.
4.  **Run `script/generate.py`**: This will generate the basic code generator files for the new language.

## 2. Template and Hook Management

1.  **Modify `src/old/bm2/gen_template/`**: For language-independent logic, you’ll need to modify these templates.
    - Example: If you need to add a new feature that affects all languages, modify the core templates in `src/old/bm2/gen_template/`.
2.  **Modify or add files in `src/old/bm2{lang_name}/hook/`**: For language-specific custom logic, edit these files based on the hook definitions. hook file is piece of C++ code that is not complete by itself. (some exception exists like flags.txt, cmptest_build.txt,cmptest_run.txt)
    - Example: Create `src/old/bm2newlang/hook/file_top.txt` to add a language-specific header to the generated files.
3.  **Check hook definitions**: Use `tool\gen_template --print-hooks` to ensure the desired hook file exists before creating it.
    - Example: Run `tool\gen_template --print-hooks` and verify that `file_top` is listed as a valid hook.
4.  **Read documentation**: `docs/template_parameters.md` for understanding placeholder usage in hooks and configuration files. Regenerate the documentation with `tool\gen_template --mode docs-markdown`.

## 3. Code Generation

1.  **Generate code**: Use `script/generate` to generate code generators and also specify a DSL file with `script/generate {brgen DSL file}`.
    - Example: Run `script/generate src/test/test_cases.bgn` to generate code generators using the `test_cases.bgn` DSL file.
2.  **Run generated generator**: Use `script/run_generated` to execute generated code and save results in `save/{lang_name}/save.{language_specific_suffix}`.
    - Example: Run `script/run_generated` to execute the `bm2newlang` code generator and save the output to `save/newlang/save.newlang`.
3.  **Check the generated code**: Manually inspect the generated code in `save/newlang/save.newlang` to ensure it is correct and follows the expected format.

## 4. Testing

The goal of test is to `PASS` the `script/run_cmptest`

1.  **Add a test case**: Add a test case file in `src/test/{test_name}.bgn`.
    - Example: Create `src/test/newlang_test.bgn` with a new test case for the `newlang` language.
2.  **Run test case**: Execute the generated test case using `script/generate src/test/{test_name}.bgn`.
    - Example: Run `script/generate src/test/newlang_test.bgn` to generate code generators using the `newlang_test.bgn` DSL file.
3.  **Run generated generator test**: Use `script/run_cmptest` to execute unit tests for generated generator. you can change input by modifying `testkit/inputs.json`. You MUST NOT write to `setup.py` directly. write python code of `setup.py` into `hook/cmptest_build.txt` instead

## 5. Debugging and Maintenance

1.  **Ignore `Unimplemented` in `src/old/bm2{lang_name}/bm2{lang_name}.cpp`**: Don't worry about `Unimplemented` unless it causes issues in `save/{lang_name}/save.{language_specific_suffix}`.
2.  **Locate hook mappings**: Use the comments `// load hook: <hook name>` and `// end hook: <hook name>` in `bm2{lang_name}/*.cpp` to map hook names to hook files.

## 6. Execution Environment

- **Use PowerShell or Bash**: Depending on the OS (Windows or Linux), use PowerShell or Bash for script execution.

# Current Development Phase

Now phase is early of project.
In some aspect, generated code may looks dirty (e.g. not good naming like tmpXXX,redundant processing) but
this kind improvement is future work, NOT NOW.

Current goal is to make code generator-generator work

Currently, you should implement inner gen_template/ first if the Unimplemented task found
