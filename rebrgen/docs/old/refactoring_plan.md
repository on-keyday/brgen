## Refactoring Plan for `src/old/bm2/gen_template` (Source Code Level)

The `gen_template` codebase, while functional, exhibits several areas of complexity primarily due to its heavy reliance on string manipulation for code generation, intertwined logic, and a flexible but deeply nested hook system. This plan aims to reduce complexity by improving internal structure, enhancing type safety, and reducing boilerplate.

**Overarching Goal:** To move away from string-based code generation towards a more structured, type-safe, and declarative approach.

### Phase 1: Improve Internal Structure and Separation of Concerns

1.  **Refactor `Flags` into Dedicated Structures:**

    - **Problem:** The `Flags` struct is a monolithic data bag serving multiple purposes (command-line options, JSON config, documentation data). This violates the Single Responsibility Principle.
    - **Solution:** Create separate, focused structs:
      - `CommandLineOptions`: For command-line parsing fields.
      - `LanguageConfig`: For all string-based configuration options (e.g., `int_type`, `block_begin`).
      - `DocumentationData`: For the `content` map and related fields used for `docs-json`/`docs-markdown` generation.
    - The `Flags` struct would then compose these smaller, more manageable structs.

2.  **Introduce a Code Generation Abstraction Layer (Intermediate Representation):**

    - **Problem:** Direct string writing to `TmpCodeWriter` is error-prone, lacks type safety, and makes the code generation logic brittle.
    - **Solution:** Define a set of C++ classes representing the target language's AST (e.g., `Statement`, `Expression`, `FunctionDecl`, `VariableDecl`).
    - Modify `write_` functions (e.g., `write_eval`, `write_inner_block`) to _return_ instances of these AST nodes instead of writing strings directly.
    - Implement a separate "renderer" component that traverses this AST and generates the final C++ strings. This clearly separates the "what to generate" from the "how to format it."

3.  **Centralize String Formatting/Templating:**

    - **Problem:** `std::format` and string concatenations are scattered throughout the codebase, making syntax changes difficult and error-prone.
    - **Solution:** Consolidate all string formatting logic into the new "renderer" component or dedicated helper functions that operate on the AST nodes. Explore using a proper templating engine (if suitable for C++) for the final string output.

4.  **Simplify Hook Invocation:**
    - **Problem:** Deeply nested `may_write_from_hook` calls with lambdas lead to "callback hell" and obscure control flow.
    - **Solution:** Implement a more explicit hook manager. Hooks would be registered with specific `AbstractOp` and `HookFileSub` combinations. `write_` functions would call a single `HookManager::invoke(op, sub, ...)` method, which would internally handle finding and executing the relevant hook. This flattens the code and clarifies default behavior.

### Phase 2: Enhance Type Safety and Reduce Boilerplate

1.  **Stronger Typing for `AbstractOp` and `StorageType` Handling:**

    - **Problem:** Large `switch` statements for `AbstractOp` and `StorageType` are repetitive and can be hard to maintain.
    - **Solution:** Explore using `std::variant` with `std::visit` if `AbstractOp` and `StorageType` can be represented as a `std::variant` of specific data structures for compile-time type safety. Alternatively, use `std::map<AbstractOp, std::function<void(...)>>` for dispatching to specific handlers, reducing `switch` statement size.

2.  **Automate Variable Definition Boilerplate:**

    - **Problem:** The `define_` functions in `define.hpp` still involve manual string formatting and conditional logic for documentation.
    - **Solution:** Create a macro or template function that takes variable name, type, description, and initial expression, automatically handling both C++ variable definition and documentation collection. This will significantly reduce repetitive `do_variable_definition` calls.

3.  **Refactor `hook_load.cpp`:**
    - **Problem:** Manual parsing of `sections.txt` and handling of hook includes is complex and brittle.
    - **Solution:** Improve parsing robustness. Consider adopting a more structured format for hooks (e.g., JSON or YAML) instead of custom `!@section` directives. This aligns with the "declarative hooks" idea from the higher-level complexity reduction plan.

### Phase 3: Long-Term Architectural Improvements

1.  **Plugin-Based Hook System:**

    - **Problem:** The current hook system, while flexible, is tightly coupled to the `gen_template` core.
    - **Solution:** Formalize the hook system into a true plugin architecture. Hooks could be dynamically loaded modules (DLLs/shared libraries) or scripts in an embedded language (e.g., Lua). This would completely decouple the `gen_template` core from language-specific logic, allowing for easier addition of new languages or modification of existing ones without recompiling the entire `gen_template`.

2.  **Dedicated AST for `BinaryModule`:**
    - **Problem:** The current processing of `rebgn::Code` objects might be a flattened representation, making complex transformations difficult.
    - **Solution:** Ensure that `rebgn::BinaryModule` is a robust and well-defined hierarchical AST. A richer AST would simplify the generation logic by providing a more structured input.

**Expected Benefits:**

- **Increased Readability:** Clearer separation of concerns, less nested logic, and more explicit data flow.
- **Improved Maintainability:** Easier to understand, debug, and modify specific parts of the code without affecting others.
- **Enhanced Type Safety:** Reduces runtime errors by leveraging C++'s type system more effectively.
- **Reduced Boilerplate:** Automates repetitive code generation tasks.
- **Greater Flexibility:** A more modular design would make it easier to extend the `gen_template` to support new languages or output formats.
