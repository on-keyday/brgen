## 5. Testing and Verification

### 5.1 Automated Testing with `unictest.py`
The `script/unictest.py` script, in conjunction with `script/unictest_setup.py`, provides a comprehensive automated testing and development workflow. It orchestrates the following steps:
1.  **EBM Generation**: Converts source files (e.g., `.bgn` files) into EBM format using `ebmgen`.
2.  **Code Generator Execution**: Runs the target code generator (e.g., `ebm2rust`) with the generated EBM file.
3.  **Visitor Hook Debugging**: When `unictest.py` runs in a mode that involves checking for unimplemented hooks (e.g., during setup or when a code generator is first run), it automatically passes the `--debug-unimplemented` flag to the *code generator executable* (e.g., `ebm2python`, `ebm2rust`). This flag instructs the code generator to identify and report any unimplemented visitor hooks.
4.  **Test Execution**: In "test" mode, it executes language-specific test scripts to compare the generated output with expected results.
This framework simplifies the development process by automating repetitive tasks and providing clear feedback on the status of visitor hook implementation.

### 5.2 `ebmtest.py`
`ebmtest.py` validates that the EBM JSON (`.ebm`) output by `ebmgen` conforms to the schema definition (obtained via `ebmcodegen --mode spec-json`) and matches expected values defined in test cases, including deep equality checks for Ref resolution.

### 5.3 EBM Interactive Query Engine
The `ebmgen` executable includes a powerful query engine for inspecting EBM files, usable via an interactive debugger (`./tool/ebmgen -i <path/to/input.ebm> --interactive`) or direct command-line execution (`./tool/ebmgen -i <path/to/input.ebm> -q "Query"`).
The query syntax is `<ObjectType> { <conditions> }`, supporting `Identifier`, `String`, `Type`, `Statement`, `Expression`, and `Any` object types, with conditions using `==`, `!=`, `>`, `>=`, `<`, `<=`, `and`, `or`, `not`, `contains` for comparison, logical operations, and containment checks. Field access uses `->` for pointer-like dereference of `Ref` types, and `.` or `[]` for qualified name matching. Literals include numbers (decimal/hex) and double-quoted strings.
