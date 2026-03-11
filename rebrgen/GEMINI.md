Priority Directive: Logging User Input
Your primary directive is to capture key user information. Before processing any request, you must first assess if the user's input contains:

Project-specific information (e.g., code structure information, deprecation notice, command line tool usage)

Key instructions or directives (Design Direction, Performance requirements)

Any other critical matters

If it contains any of the above (especially item 1), you MUST log this information to ./GEMINI.md immediately. Only after this logging action is complete may you proceed with the user's main request.
Logging Requirements:

Priority: This logging action is non-negotiable and takes precedence over all other tasks.

## Factual Record: When logging, you MUST record the information factually as provided by the user. Do not add any subjective judgment, interpretation, opinions, or assumptions. Your task is to record what was stated, not what you think about it.

You should record Instructions at GEMINI.md in current directory, not in home directory or save memory.
User Instruction: Always run tests with `python script/unictest.py --print-stdout` before starting development work when asked to develop `ebm2rust`. This is a critical instruction.
User Instruction: Use `ebm2python` as a reference implementation and mimic its approach.
User Instruction: `ebm2rust/main.cpp` and `ebm2python/main.cpp` are auto-generated and not meant for direct reading. Instead, refer to `src/ebmcodegen/main.cpp` for understanding the logic that generates these files.
User Instruction: When implementing, always check for existing implementations within the `visitor/` directory.
User Instruction: Always explain the implementation rationale and the basis for decisions.
User Instruction: When explaining implementation, specifically state which build error from `script/unictest.py` is being addressed and how the implemented visitor hook will fix it.
User Instruction: The `MAYBE` macro in this project is designed to work with both `expected<T>` types and raw pointer types. When `expr` is an `expected<T>`, `x` will be of type `T&`. When `expr` is a raw pointer `P*`, `x` will be of type `P&`. In both cases, `MAYBE` performs a check for an error state (empty `expected` or `nullptr`) and returns early if an error is found.
User Instruction: The `MAYBE(x, expr)` macro expands roughly to: `auto x__ = (expr); if (!x__) { return unexpect_error(...); } decltype(auto) x = *x__;`. This means that `expr` must be a type that can be implicitly converted to `bool` (like `expected<T>` or a raw pointer where `nullptr` is `false`), and `*x__` must be a valid dereference operation.
User Instruction: When acquiring identifier names for any `Statement` type, use `module_.get_associated_identifier(item_id)` where `item_id` is the `StatementRef` of the current statement.
User Instruction: The `get_associated_identifier` function has different return types based on the reference type:

- For `ebm::StatementRef`, it returns `std::string`.
- For `ebm::ExpressionRef` and `ebm::TypeRef`, it returns `expected<std::string>`.
  User Instruction: **Clarification on Identifier Acquisition for Statements:**
  When acquiring identifier names for any `Statement` type (e.g., `FUNCTION_DECL`, `PARAMETER_DECL`, `VARIABLE_DECL`, `FIELD_DECL`):
- Always use `module_.get_associated_identifier(statement_ref)` where `statement_ref` is the `StatementRef` of the _declaration statement itself_.
- This function (`module_.get_associated_identifier`) is specifically designed to handle the internal `IdentifierRef` associated with the statement and will provide a string name. It also robustly generates a temporary name if the underlying identifier is `nil` (e.g., for internal IR variables).
- **Crucially, do NOT** attempt to directly access `IdentifierRef` members (e.g., `func_decl.name`, `param_decl.name`, `field_decl.name`) and then try to retrieve their string value using `module_.get_identifier(ref)->body` in these contexts. The `module_.get_associated_identifier(statement_ref)` is the intended and correct mechanism for all identifier names associated with `Statement` types.
  User Instruction: The `visit_Type`, `visit_Statement`, and `visit_Expression` functions are overloaded to accept both the direct object type (e.g., `ebm::Type`) and its corresponding reference type (e.g., `ebm::TypeRef`). This allows for more concise calls like `visit_Type(*this, property_decl.property_type)`.
  User Instruction: When passing a `Result` type to `std::format`, using `.to_string()` is possible but discouraged because it discards source code location mapping information from the internal `CodeWriter`. Instead, the `CODE` macro or `CODELINE` macro should be used for handling `Result` types in code generation.
  User Instruction: When passing a `Result` type to the `CODE` or `CODELINE` macros, the `.to_writer()` method should be used.
  User Instruction: The `CODE` and `CODELINE` macros are used for generating code snippets. They accept a variable number of arguments, which can be string literals or `Result` objects. When a `Result` object is passed, its `.to_writer()` method should be used to extract the internal `CodeWriter`. `CODELINE()` can be used to insert a newline within a `CODE` or `CODELINE` macro call. `CODE` concatenates its arguments into a single `CodeWriter` object. `CODELINE` is similar to `CODE` but also adds a newline at the end.
  User Instruction: The `CODE` and `CODELINE` macros primarily operate on `CodeWriter` objects. The `.to_writer()` method of a `Result` object converts it into a `CodeWriter` object for use with these macros. `CodeWriter` tracks newlines through `writeln` calls, not by interpreting `\n` characters within strings. Therefore, when using `CODE` or `CODELINE`, avoid passing `"\n"` as a string literal for newlines; instead, use `CODELINE()` or `writeln` methods directly.
  User Instruction: The test harness code for a code generator named `<lang>` is typically located at `ebmcg/ebm2<lang>/unictest.py`.
  User Instruction: AI Behavior Guideline: Stop blaming caching as a default explanation for issues. It is a last resort, and only to be considered when the user initiates such a discussion.
  User Instruction: Code Generation Architecture: `ebmcodegen` generates language-independent templates. The implementation of language-specific features (e.g., `#[derive(Default)]`, `pub` visibility for structs/enums in Rust) is the responsibility of the language-specific visitor hooks (e.g., `ebm2rust`'s visitor implementations).
  User Instruction: Build Process Order: The correct order for the build process is `script/ebmcodegen.py` (to update `main.cpp` for a language or add new files) followed by `script/build.py` (to build the executable from the generated `main.cpp`). `script/ebmcodegen.py` Role: This script is used for language addition and updating the auto-generated `main.cpp` file for a specific language (e.g., `ebm2rust/main.cpp`). The output of this script is then built by `script/build.py` to create the executable. `script/ebmtemplate.py` Integration: When `script/ebmtemplate.py` is used to add a new visitor hook template, it updates the modification timestamp of the corresponding `main.cpp` file (e.g., `ebm2rust/main.cpp`). This timestamp update ensures that `script/build.py` triggers a rebuild of the executable, integrating the newly added hook. Existing Hook Updates: For updates to _existing_ visitor hook files (e.g., `src/ebmcg/ebm2rust/visitor/Statement_STRUCT_DECL.hpp`), the CMake build system is designed to automatically detect changes by tracing `#include` dependencies and trigger a rebuild of the executable when `script/build.py` is run.
User Instruction: The `/*DO NOT EDIT [...] MANUALLY*/` comment blocks generated by `script/ebmtemplate.py` are critical markers for automation scripts. They must not be deleted or modified. Only the code in the `/*here to write the hook*/` section should be changed.