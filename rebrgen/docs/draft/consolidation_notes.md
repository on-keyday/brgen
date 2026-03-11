# Documentation Consolidation Notes

## 1. Project Overview

### 1.1 Project Goal & Purpose
The `rebrgen` project is a generator construction project for `brgen` (https://github.com/on-keyday/brgen). While the original `brgen` code generator uses an AST-to-Code model, `rebrgen` adopts an AST-to-IR-to-Code model for improved flexibility and inter-language compatibility.
This subproject, `ebmgen`, is part of the IR project and is the successor to `bmgen` and `BinaryModule(bm)`. It converts the `brgen` Abstract Syntax Tree (AST) into a superior Intermediate Representation (IR) called the **`ExtendedBinaryModule` (EBM)**.
The `ebmcodegen` subproject is a code-generator-generator, which scans the `ExtendedBinaryModule` with a reflection mechanism based on a visitor pattern and generates C++ code that forms the backbone of code generators.

### 1.2 Workflow Overview
The overall workflow involves:
1.  A `.bgn` file is processed by `src2json` (from the `brgen` repository) to produce a `brgen` AST (JSON).
2.  The `brgen` AST (JSON) is input to `ebmgen`, which generates the EBM.
3.  The EBM is then processed by a language-specific code generator (built using `ebmcodegen`) to produce code in various languages.
4. If the `.bgn` file is `extended_binary_module.bgn` itself, `json2cpp2` (from the `brgen` repository) generates `extended_binary_module.hpp`/`.cpp` files, which are integrated with `ebmcodegen` to generate the C++ source code for the code generator.

### 1.3 The EBM: A Superior Intermediate Representation
The EBM (`src/ebm/extended_binary_module.bgn`) features a graph-based structure with centralized data tables where objects (Statements, Expressions, Types, etc.) are referenced by unique IDs. It supports structured control flow (if, loop, match), higher-level abstraction (focusing on 'what' rather than 'how'), lowered statements for a bridge to lower-level representations, and preserves critical binary format details like endianness and bit size.

## 2. Setting Up the Development Environment

### 2.1 Build Instructions
1.  Copy `build_config.template.json` to `build_config.json`.
2.  If you want to build `brgen` tools, set `AUTO_SETUP_BRGEN` to `true` in `build_config.json`.
3.  Initially, run `python script/auto_setup.py`. For subsequent builds, run `python script/build.py native Debug`. This command will build the project in `Debug` mode for your native platform. Executables will be located at `tool/ebmgen.exe`, `tool/ebmcodegen.exe`, etc. (on Windows) or corresponding paths on other OSes.

## 3. Working with EBM and Code Generators (ebmgen/ebmcodegen)

### 3.1 Running `ebmgen`
`ebmgen` can take various input formats (brgen AST JSON, `.bgn` file, EBM binary) and produce an EBM file. It supports debugging and query capabilities.

**Basic Conversion:**
-   Convert a `brgen` AST JSON to an EBM binary file: `./tool/ebmgen -i <path/to/input.json> -o <path/to/output.ebm>`
-   Convert a `.bgn` file directly (requires `libs2j`): `./tool/ebmgen -i <path/to/input.bgn> -o <path/to/output.ebm>`

**Debugging:**
-   Output a human-readable text dump of the EBM: `./tool/ebmgen -i <path/to/input.ebm> -d <path/to/debug_output.txt>`
-   Output the EBM structure as a JSON file: `./tool/ebmgen -i <path/to/input.ebm> -d <path/to/debug_output.json> --debug-format json`

**Common Command-Line Options:**
| Flag             | Alias | Description                                                                                                             |
| ---------------- | ----- | ----------------------------------------------------------------------------------------------------------------------- |
| `--input`        | `-i`  | **(Required)** Specifies the input file.                                                                                |
| `--output`       | `-o`  | Specifies the output EBM binary file. Use `-` for stdout.                                                               |
| `--debug-print`  | `-d`  | Specifies a file for debug output. Use `-` for stdout.                                                                  |
| `--input-format` |       | Explicitly sets the input format. One of: `bgn`, `json-ast`, `ebm`. Defaults to auto-detection based on file extension. |
| `--debug-format` |       | Sets the format for debug output. One of: `text` (default), `json`.                                                     |
| `--interactive`  | `-I`  | Starts the interactive debugger after loading the input file.                                                           |
| `--query`        | `-q`  | Executes a query directly from the command line.                                                                        |
| `--query-format` |       | Sets the output format for the `--query` flag. One of: `id` (default), `text`, `json`.                                  |
| `--cfg-output`   | `-c`  | Outputs the Control Flow Graph (CFG) to the specified file.                                                             |
| `--libs2j-path`  |       | Specifies the path to the `libs2j` dynamic library for converting `.bgn` files.                                         |
| `--debug`        | `-g`  | Enables debug transformations, such as not removing unused items from the EBM.                                          |
| `--verbose`      | `-v`  | Enables verbose logging.                                                                                                |
| `--timing`       |       | Prints processing time for each major step.                                                                             |
| `--base64`       |       | Output as base64 encoding (for web playground compatibility).                                                           |
| `--output-format`|       | Output format (default: binary).                                                                                        |
| `--show-flags`   |       | Output command line flag description in JSON format.                                                                    |

### 3.2 Running `ebmcodegen`
`ebmcodegen` is a code-generator-generator. Its primary use is to create the C++ template for a new language-specific code generator.
-   Create a new generator template for a language named "my_lang": `python ./script/ebmcodegen.py my_lang`
This command utilizes `./tool/ebmcodegen` and creates a new directory at `src/ebmcg/ebm2my_lang/` containing the skeleton for the new generator.

**Common Command-Line Options:**
| Flag                                                                 | Alias | Description                                                                                                             |
| -------------------------------------------------------------------- | ----- | ----------------------------------------------------------------------------------------------------------------------- |
| `--help, -h`                                                         |       | show this help                                                                                                          |
| `-l=LANG, --lang=LANG`                                               |       | language for output                                                                                                     |
| `-p=PROGRAM_NAME, --program-name=PROGRAM_NAME`                       |       | program name (default: ebm2{lang})                                                                                      |
| `-d=DIR, --visitor-impl-dir=DIR`                                     |       | directory for visitor implementation                                                                                    |
| `--visitor-impl-dsl-dir=DIR`                                         |       | directory for visitor implementation that is generated from DSL                                                         |
| `--default-visitor-impl-dir=DIR`                                     |       | directory for default visitor implementation                                                                            |
| `--template-target=target_name`                                      |       | template target name. see --mode hooklist                                                                               |
| `--dsl-file=FILE`                                                    |       | DSL source file for --mode dsl                                                                                          |
| `--mode={subset,codegen,interpret,hooklist,hookkind,template,spec-json,dsl}` |       | generate mode (default: codegen)                                                                                        |

### 3.3 Updating the EBM Structure with `script/update_ebm.py`
The `script/update_ebm.py` script automates the entire process of regenerating EBM-related files when the `extended_binary_module.bgn` structure is modified. This ensures consistency across the project. The script performs the following actions:
1.  **Updates C++ EBM Definition**: Executes `src/ebm/ebm.py` to regenerate `src/ebm/extended_binary_module.hpp` and `src/ebm/extended_binary_module.cpp` from `src/ebm/extended_binary_module.bgn`.
2.  **Initial Build**: Runs `script/build.py` to build the project with the updated EBM C++ definitions.
3.  **Generates `body_subset.cpp`**: Uses `tool/ebmcodegen --mode subset` to create `src/ebmcodegen/body_subset.cpp`, which contains metadata about EBM structures.
4.  **Generates JSON Conversion Headers**: Uses `tool/ebmcodegen --mode json-conv-header` to create `src/ebmgen/json_conv.hpp` for JSON deserialization.
5.  **Generates JSON Conversion Source**: Uses `tool/ebmcodegen --mode json-conv-source` to create `src/ebmgen/json_conv.cpp` for JSON deserialization.
6.  **Conditional Rebuild**: If any of the generated files (steps 3-5) have changed, the script runs `script/build.py` again to ensure all tools are up-to-date.
7.  **Generates Hex Test Data**: Converts `src/ebm/extended_binary_module.bgn` into a hexadecimal format and saves it to `test/binary_data/extended_binary_module.dat`, which can be used for testing.
To update the EBM structure and all dependent generated files, simply run: `python script/update_ebm.py`

## 4. Code Generation Logic (Visitor Hooks)

### 4.1 Visitor Hook Overview
A visitor hook is a `.hpp` file containing a C++ code snippet. It is **not** a class definition. Instead, it's the literal body of a function that is dynamically generated by `ebmcodegen`. `ebmcodegen` generates functions like `visit_Statement_WRITE_DATA(...)`, and the content of your `.hpp` file (e.g., `visitor/Statement_WRITE_DATA.hpp`) is `#include`d directly into the body of that function. The function, and therefore your hook code, **must** return a value of type `expected<Result>`. The `Result` struct holds the generated code string in its `value` member.

### 4.2 Managing Visitor Hooks with `ebmtemplate.py`
The `script/ebmtemplate.py` script is a wrapper around `ebmcodegen` that simplifies the lifecycle of visitor hooks.
-   **`python script/ebmtemplate.py`**: Displays help.
-   **`python script/ebmtemplate.py interactive`**: Start an interactive guide through the script features.
-   **`python script/ebmtemplate.py <template_target>`**: Outputs a summary of the hook file to stdout.
-   **`python script/ebmtemplate.py <template_target> <lang>`**: Generates a new hook file (`.hpp`) in `src/ebmcg/ebm2<lang>/visitor/`.
-   **`python script/ebmtemplate.py update <lang>`**: Updates the auto-generated comment blocks (listing available variables) in all existing hooks for a given language.
-   **`python script/ebmtemplate.py test`**: Tests the generation of all available template targets.
-   **`python script/ebmtemplate.py list <lang>`**: List all defined templates in the specified [lang] directory.

### 4.3 Workflow for Implementing a Visitor Hook
1.  **Find the Target**: Decide which EBM node you want to handle. Use `tool/ebmcodegen --mode hooklist` to list available hooks.
2.  **Create the File**: Generate the hook file using `python script/ebmtemplate.py <template_target> <lang>`.
3.  **Implement the Logic**: Open the newly created file (`src/ebmcg/ebm2<lang>/visitor/<template_target>.hpp`). Replace the `/*here to write the hook*/` comment with your C++ implementation.
4.  **Build and Verify**: After implementing the hook, run `python script/unictest.py` (or `python script/unictest.py --print-stdout` for debugging) to build and verify your changes. This will trigger the build process and run the tests defined in the `unictest` framework. Also, remember to update `src/ebmcg/ebm2<lang>/main.cpp`'s Last updated time at creation of hook file to make the build system recognize the new file.

### 4.4 DSL (Domain Specific Language) for Visitor Hooks
The `ebmcodegen` tool supports a Domain Specific Language (DSL) for writing visitor hooks, offering a more concise and readable way to define code generation logic. This DSL is processed by `ebmcodegen` when invoked with `--mode dsl` and `--dsl-file=FILE`.

**DSL Syntax Overview:**
The DSL mixes C++ code, EBM node processing, and control flow constructs using special markers:
-   **`{% C++_CODE %}`**: Embeds C++ literal code directly into the generated output. (e.g., `{% int a = 0; %}`)
-   **`{{ C++_EXPRESSION }}`**: Embeds a C++ expression whose result is written to the output. (e.g., `{{ a }} += 1;`)
-   **`{* EBM_NODE_EXPRESSION *}`**: Processes an EBM node (e.g., `ExpressionRef`, `StatementRef`) by calling `visit_Object` and writing its generated output. (e.g., `{* expr *}`)
-   **`{& IDENTIFIER_EXPRESSION &}`**: Retrieves an identifier and writes it to the output. (e.g., `{& item_id &}`)
-   **`{! SPECIAL_MARKER !}`**: Used for advanced control flow and variable definitions within the DSL itself. The content inside these markers is parsed by a nested DSL.
    -   **`transfer_and_reset_writer`**: Generates C++ code to transfer the current `CodeWriter` content and reset it.
    -   **`for IDENT in (range(BEGIN, END, STEP) | COLLECTION)`**: Generates C++ `for` loops. Supports both numeric ranges and iteration over collections.
    -   **`endfor`**: Closes a `for` loop block.
    -   **`if (CONDITION)` / `elif (CONDITION)` / `else`**: Generates C++ `if`/`else if`/`else` blocks.
    -   **`endif`**: Closes an `if` block.
    -   **`VARIABLE := VALUE`**: Defines a C++ variable within the generated code. (e.g., `my_var := 42`)

Text outside these markers is treated as target language code and is escaped before being written to the output. The DSL also handles automatic indentation and dedentation based on the source formatting.

## 5. Testing and Verification

### 5.1 Automated Testing with `unictest.py`
The `script/unictest.py` script, in conjunction with `script/unictest_setup.py`, provides a comprehensive automated testing and development workflow. It orchestrates the following steps:
1.  **EBM Generation**: Converts source files (e.g., `.bgn` files) into EBM format using `ebmgen`.
2.  **Code Generator Execution**: Runs the target code generator (e.g., `ebm2rust`) with the generated EBM file.
3.  **Visitor Hook Debugging**: When run in "setup" mode, it uses the `--debug-unimplemented` flag to identify and report any unimplemented visitor hooks in the code generator.
4.  **Test Execution**: In "test" mode, it executes language-specific test scripts to compare the generated output with expected results.
This framework simplifies the development process by automating repetitive tasks and providing clear feedback on the status of visitor hook implementation.

### 5.2 `ebmtest.py`
`ebmtest.py` validates that the EBM JSON (`.ebm`) output by `ebmgen` conforms to the schema definition (obtained via `ebmcodegen --mode spec-json`) and matches expected values defined in test cases, including deep equality checks for Ref resolution.

### 5.3 EBM Interactive Query Engine
The `ebmgen` executable includes a powerful query engine for inspecting EBM files, usable via an interactive debugger (`./tool/ebmgen -i <path/to/input.ebm> --interactive`) or direct command-line execution (`./tool/ebmgen -i <path/to/input.ebm> -q "Query"`).
The query syntax is `<ObjectType> { <conditions> }`, supporting `Identifier`, `String`, `Type`, `Statement`, `Expression`, and `Any` object types, with conditions using `==`, `!=`, `>`, `>=`, `<`, `<=`, `and`, `or`, `not`, `contains` for comparison, logical operations, and containment checks. Field access uses `->` for pointer-like dereference of `Ref` types, and `.` or `[]` for qualified name matching. Literals include numbers (decimal/hex) and double-quoted strings.

## 6. Important Concepts and Guidelines

### 6.1 Macro Usage (MAYBE macro)
The codebase extensively uses C++ macros, particularly the `MAYBE` macro for error handling. This is a deliberate design choice aimed at improving readability and maintainability by making error handling transparent, similar to Haskell's do notation or Rust's `?` operator. The `MAYBE(x, expr)` macro handles error states (empty `expected` or `nullptr`) and performs early returns, all while preserving RAII and allowing for flexible, context-aware error handling via `Error::as<U>()`.

### 6.2 Error Fix Strategy
When encountering compile errors like `undefined symbol` or `type mismatch`, it is crucial to consult the definition (especially type definitions) in `extended_binary_module.hpp`, `mapping.hpp`, and other relevant `.hpp` files. Avoid guessing; always re-load data using `ReadFile` action if necessary. Macro expansions should generally be ignored unless directly implicated in the error message, as the issue usually lies in a misunderstanding of the definition itself.

### 6.3 Action Mental Model
When developing, always start by running `ebm2<lang name>` (or `python script/unictest.py`) to check the current stage. Debugging typically involves finding corresponding visitor/code for TODOs or missing fragments. For compile errors, prioritize checking definitions in `extended_binary_module.hpp`, `mapping.hpp`, and other `.hpp` files over making assumptions or relying on cached knowledge.

### 6.4 Code Generation Architecture
`ebmcodegen` generates language-independent templates. Language-specific features (e.g., `#[derive(Default)]`, `pub` visibility in Rust) are implemented in language-specific visitor hooks (e.g., `ebm2rust`'s visitors).

### 6.5 Build Process Order
The correct build order is `script/ebmcodegen.py` (for language addition or updating `main.cpp`) followed by `script/build.py`. `script/ebmcodegen.py` updates the auto-generated `main.cpp` for a specific language, which `script/build.py` then uses to create the executable. `script/ebmtemplate.py` updates `main.cpp`'s timestamp when creating a new visitor hook, ensuring `script/build.py` rebuilds the executable. Changes to *existing* visitor hook files are automatically detected by CMake through `#include` dependencies, triggering a rebuild when `script/build.py` is run.

## 7. Script Reference

### Build Related
-   **`build.py` / `build.ps1` / `build.bat` / `build.sh`**: Project build scripts. `build.py` manages `cmake` and `ninja` for C++ builds (`native`, `web` modes). Others are wrappers.

### Code Generation & Template Related
-   **`ebmcodegen.py`**: Wraps `tool/ebmcodegen` to generate skeleton code (`main.cpp`, `CMakeLists.txt`) for new EBM-based code generators/interpreters in `src/ebmcg/ebm2<lang>`.
-   **`ebmtemplate.py`**: Helper script for managing visitor hook template files (create, update comments, test generation) using `ebmcodegen`.
-   **`ebmwebgen.py`**: Automatically generates JavaScript glue code (e.g., `ebm_caller.js` in `web/tool`) for running EBM-based code generators as Web applications.
-   **`script/ebmdsl.py`**: Automates the compilation of `.dsl` files into C++ visitor hook implementations. It iterates through language directories in `src/ebmcg/`, processes `.dsl` files found in their `dsl` subdirectories using `tool/ebmcodegen --mode dsl`, and writes the generated C++ code to corresponding `.hpp` files in `visitor/dsl`.
-   **`src/ebm/ebm.ps1`**: (located in `src/ebm`) PowerShell script to generate `extended_binary_module.hpp` and `extended_binary_module.cpp` from `extended_binary_module.bgn`. Similar functionality to `src/ebm/ebm.py`.

### Test & Execution Related
-   **`ebmtest.py`**: Validates `ebmgen`'s EBM JSON output against schema and test case expectations.
-   **`unictest.py`**: Orchestrates automated testing and development workflow by running `ebmgen`, code generators, and test scripts (see Section 5.1).
-   **`run_cycle.py` / `run_cycle.ps1` / `run_cycle.bat` / `run_cycle.sh`**: Old generation. Executes entire development cycle: `generate.py`, `run_generated.py`, `run_cmptest.py`.

### Utilities
-   **`split_dot.py`**: Old generation. Divides large DOT graph files (generated by `ebmgen`) into subgraphs as separate DOT and SVG files for better visualization.

### Old Generation Scripts (References for historical context, not current use)
-   **`gen_template.py` / `gen_template.ps1` / `gen_template.bat` / `gen_template.sh`**: For `bmgen`, generates language code generator templates (`.hpp`, `.cpp`, `main.cpp`, `CMakeLists.txt`) in `src/old/bm2<lang>/`.
-   **`generate.py` / `generate.ps1` / `generate.bat` / `generate.sh`**: For `bmgen`, orchestrates overall code generation and configuration updates (e.g., `gen_template.py`, `collect_cmake.py`, `generate_test_glue.py`).
-   **`collect_cmake.py`**: For `bmgen`, automatically generates `src/CMakeLists.txt` by adding `add_subdirectory` for `bm2<lang>` directories and generates `run_generated.py`.
-   **`generate_web_glue.py`**: For `bmgen`, generates JavaScript glue code for web applications.
-   **`run_generated.py` / `run_generated.ps1` / `run_generated.bat` / `run_generated.sh`**: For `bmgen`, automatically built by `collect_cmake.py`. Builds project, converts `.bgn` to JSON AST, runs `bmgen`, and then runs language generators.
-   **`run_cmptest.py` / `run_cmptest.ps1` / `run_cmptest.bat` / `run_cmptest.sh`**: For `brgen`, runs `cmptest` tool for compatibility testing based on `testkit/` configurations.
-   **`generate_test_glue.py`**: For `bmgen`, generates `cmptest` test configuration files (`cmptest.json`, `test_info.json`) in `testkit/`.
-   **`generate_golden_master.py`**: For `bmgen`, generates all language generator source code using `gen_template` and saves to `test/golden_masters/` for "golden master" testing.
-   **`test_compatibility.py`**: For `bmgen`, compares `gen_template`'s current output with golden masters to check for backward compatibility.
-   **`binary_module.bat` / `binary_module.ps1`**: For `bmgen`, generates C++ headers and source files from `binary_module.bgn` using `brgen`'s `src2json` and `json2cpp2`.
-   **`list_lang.py`**: For `bmgen`, lists supported languages by exploring `bm2<lang>` directories in `src/`.

## 8. Inconsistencies/Notes for Review

### 8.1 "How to Add Language" in `README.md`
The "How to Add Language" section in the top-level `README.md` describes a manual, step-by-step process involving `src2json`, `ebmgen`, and `ebm2<lang name>` executions. This approach is partially outdated as the `script/unictest.py` framework now automates much of this process, especially for testing and verifying new language implementations. The new documentation should emphasize the `unictest.py` approach as the primary workflow for developing and testing language generators, while keeping the underlying manual command sequence as an explanation of the internal mechanism or for specific debugging scenarios.

### 8.2 General Note on "Old Generation" Scripts
Many scripts in `commands.md` are marked as `[旧世代]bmgen`. The new documentation should clearly separate and de-emphasize these older scripts, focusing primarily on the `ebmgen`/`ebmcodegen` and `unictest.py` related tools and workflows.

### 8.3 Location of `run_cycle.py`
The `commands.md` file lists `run_cycle.py` as an old generation script located in `script/`. However, the script is actually located at `script/bm/run_cycle.py`. This indicates an incorrect location in `commands.md`.

### 8.4 Hardcoded `TOOL_PATH` in `src/ebm/ebm.py`
**Resolved**: The hardcoded absolute path for `TOOL_PATH` in `src/ebm/ebm.py` has been fixed by the user. This issue is no longer present.
