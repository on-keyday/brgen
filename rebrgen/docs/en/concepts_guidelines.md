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
