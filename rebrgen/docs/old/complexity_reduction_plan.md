### Plan to Reduce Complexity in `rebrgen`

This plan is divided into three phases, starting with the least disruptive changes and progressing to a more fundamental refactoring.

**Phase 1: Documentation and Visibility (Short-Term)**

The immediate goal is to make the existing system easier to understand without changing its logic. The primary problem is the "implicit contract" between the `gen_template` and the hooks.

1.  **Automated Hook Documentation:**
    *   **Action:** Create a script (e.g., `script/document_hooks.py`) that analyzes the `gen_template` source code.
    *   **Goal:** The script will find all `// load hook:` comments and extract:
        *   The name of the hook.
        *   The C++ variables and functions available in the scope where the hook is loaded.
        *   The location in the `gen_template` source where it's used.
    *   **Output:** A generated markdown file, `docs/hooks_reference.md`, that serves as an explicit reference for anyone writing or debugging hooks.

2.  **Create a Developer's Guide:**
    *   **Action:** Write a new document, `docs/developer_guide.md`.
    *   **Goal:** This guide will provide a step-by-step walkthrough for common development tasks, such as:
        *   "How to add a new language feature to all generators."
        *   "How to trace a bug from generated code back to the template/hook."
        *   "The lifecycle of a `BinaryModule` object during code generation."

**Phase 2: Unification and Consistency (Mid-Term)**

The goal of this phase is to reduce the number of different systems a developer needs to understand.

1.  **Migrate Handwritten Generators:**
    *   **Action:** Refactor the handwritten `bm2rust` and `bm2cpp` generators to use the `gen_template` system.
    *   **Goal:** Eliminate the special cases. This ensures that all language generators follow the same pattern, making maintenance and feature addition consistent across all languages. All development effort can be focused on improving the `gen_template` and hook system.

2.  **Standardize and Validate Configuration:**
    *   **Action:** Define a JSON Schema for the `config.json` files.
    *   **Goal:** Enforce a consistent structure for all language configurations. This allows for automated validation and makes it clear what configuration options are available, preventing typos and structural errors. The `generate` script can be updated to validate all `config.json` files against this schema.

**Phase 3: Modernizing the Hook System (Long-Term Vision)**

This is the most significant phase and aims to replace the most complex part of the system—the raw C++ hooks—with a more structured and safer approach.

1.  **Introduce Declarative Hooks (for simple cases):**
    *   **Action:** Modify `gen_template` to read structured data (e.g., from a new `hooks.json` file in each `bm2{lang_name}` directory).
    *   **Goal:** Replace simple, repetitive C++ hooks with a declarative format. Many hooks are just for mapping types or defining language keywords.
    *   **Example:**
        *   **Before (in a `.txt` hook):** `out << "uint32_t";`
        *   **After (in `hooks.json`):** `{ "type_mapping": { "u32": "uint32_t" } }`

2.  **Adopt a Scripting Language for Complex Hooks:**
    *   **Action:** Embed a lightweight scripting language interpreter (like Lua or a similar sandboxed language) into `gen_template`.
    *   **Goal:** Replace complex C++ hooks with scripts that interact with the generator through a well-defined, safe API. This eliminates the risks of arbitrary C++ injection and makes the logic much easier to write and reason about. The script would be passed a context object with the necessary data and would return the generated code as a string.
    *   **Benefits:**
        *   **Safety:** Scripts are sandboxed and cannot corrupt the generator's state.
        *   **Simplicity:** The API provides a clear contract, removing ambiguity.
        *   **Accessibility:** Developers wouldn't need to be C++ experts to modify language-specific logic.

By executing this plan, the `rebrgen` project can be progressively transformed from a system that requires deep, implicit knowledge into one that is explicit, consistent, and more maintainable for a wider range of contributors.