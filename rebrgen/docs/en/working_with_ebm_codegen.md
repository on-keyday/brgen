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
