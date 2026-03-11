# Project Directory Structure

## 1. Core Directories

| Directory        | Description                           | Notes                                                          |
| ---------------- | ------------------------------------- | -------------------------------------------------------------- |
| `brgen/`         | Base `brgen` directory as a submodule | Same as [brgen repository](https://github.com/on-keyday/brgen) |
| `brgen/example/` | Example set of `brgen` DSL            |                                                                |
| `src/`           | Source code                           |                                                                |

## 2. Binary Module (`bm`)

| Directory / File                                                | Description                                                     | Notes                                                                       |
| --------------------------------------------------------------- | --------------------------------------------------------------- | --------------------------------------------------------------------------- |
| `src/old/bm/`                                                   | Binary module definition                                        |                                                                             |
| `src/old/bm/binary_module.bgn`                                  | Base definition for `binary_module.hpp` and `binary_module.cpp` | Defines `BinaryModule` structure and `AbstractOp` enum for code generation. |
| `src/old/bm/binary_module.hpp` / `src/old/bm/binary_module.cpp` | Generated files from `binary_module.bgn`                        |                                                                             |

## 3. Binary Module Generator (`bmgen`)

| Directory / File           | Description                                                                                        | Notes |
| -------------------------- | -------------------------------------------------------------------------------------------------- | ----- |
| `src/old/bmgen/`           | Converts `brgen` AST (`brgen/web/doc/content/docs/ast.md`) to `BinaryModule` (`binary_module.bgn`) |       |
| `src/old/bmgen/transform/` | Binary Module transformation logic                                                                 |       |
| `src/old/bmgen/main.cpp`   | Entry point                                                                                        |       |

## 4. Generator (`bm2`)

| Directory / File                           | Description                                    | Notes                                                                                                                                        |
| ------------------------------------------ | ---------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- | ------------------- |
| `src/old/bm2/`                             | Common generator tools and generator-generator |                                                                                                                                              |
| `src/old/bm2/gen_template/`                | Generator-generator source code                | Includes entry point.                                                                                                                        |
| `src/old/bm2{lang_name}/`                  | Generator for `{lang_name}`                    | Run `script/list_lang.py` to list available languages. These generate code from `BinaryModule`. \*\*DO NOT edit `src/old/bm2{lang*name}/*.[h | c]pp` directly.\*\* |
| `src/old/bm2hexmap/`                       | Maps hex representation to format              | Not a code generator.                                                                                                                        |
| `src/old/bm2rust/`, `src/old/bm2cpp/`      | Hand-written generators                        | Created before generator-generator was available.                                                                                            |
| `src/old/bm2{lang_name}/hook/`             | Hook directory for each language               |                                                                                                                                              |
| `src/old/bm2{lang_name}/hook/sections.txt` | main hook file if exists                       |
| `src/old/bm2{lang_name}/config.json`       | Placeholder & basic language configurations    |                                                                                                                                              |

## 5. Testing (`test`)

| Directory / File          | Description                          | Notes |
| ------------------------- | ------------------------------------ | ----- |
| `src/test/`               | Test input files (`*.bgn`)           |       |
| `src/test/test_cases.bgn` | Main test cases                      |       |
| `src/test/crash/`         | Files that previously caused crashes |       |

## 6. Scripts (`script`)

| Directory / File       | Description                                                                                                       | Notes                                                                 |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| `script/`              | Build scripts, code generation, etc.                                                                              | Scripts have `.ps1`, `.bat`, `.sh` or `.py` suffixes. use `.py` prior |
| `script/binary_module` | Generates `binary_module.hpp` and `binary_module.cpp`                                                             |                                                                       |
| `script/list_lang.py`  | Lists available languages                                                                                         |                                                                       |
| `script/build`         | Build tools                                                                                                       |                                                                       |
| `script/generate`      | Builds generator-generator (`tool/gen_template`) & generates language-specific generators (`tool/bm2{lang_name}`) |                                                                       |
| `script/run_generated` | Builds & runs code generators (`tool/bm2{lang_name}`)                                                             |                                                                       |
| `script/run_cmptest`   | Runs test cases                                                                                                   |                                                                       |
| `script/run_cycle`     | Runs `generate`, `run_generated`, then `run_cmptest`                                                              |                                                                       |

## 7. Generated & Temporary Files

| Directory / File    | Description                 | Notes                                                                                      |
| ------------------- | --------------------------- | ------------------------------------------------------------------------------------------ |
| `save/`             | Temporary & testing files   | Ignored by `.gitignore`.                                                                   |
| `save/{lang_name}/` | Generated code per language |                                                                                            |
| `tool/`             | Built tools                 | Names match `src/{dir_name}`. These are executables, not scripts. Ignored by `.gitignore`. |
| `web/`              | WebPlayground tools         | Ignored by `.gitignore`.                                                                   |

## 8. Documentation & Testing Framework

| Directory / File       | Description                                | Notes                                        |
| ---------------------- | ------------------------------------------ | -------------------------------------------- |
| `docs/`                | Documentation files                        | Some are AI-generated and may be inaccurate. |
| `testkit/`             | Testing framework for generated generators | Auto-generated by `script/generate`.         |
| `testkit/inputs.json`  | Test input definitions                     |                                              |
| `testkit/cmptest.json` | Auto-generated runner input                | Copy of `inputs.json`.                       |
