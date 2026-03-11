# AGENTS.md

Operational guide for AI coding agents working in this repository.

## Critical Warnings

- **Windows environment.** NEVER redirect to `nul` (e.g., `2>nul`). This creates a literal file named `nul` on Windows. Use `2>/dev/null` instead, or omit redirection entirely.
- **NEVER edit auto-generated files:**
  - `src/ebmcg/ebm2<lang>/main.cpp`
  - `src/ebmcg/ebm2<lang>/codegen.hpp`
  - `src/ebmcodegen/body_subset.cpp`
  - `src/ebmgen/json_conv.cpp`, `json_conv.hpp`
  - `src/ebm/extended_binary_module.cpp`, `extended_binary_module.hpp`
  - Files delimited by `/*DO NOT EDIT BELOW/ABOVE SECTION MANUALLY*/` — only edit outside those markers.
- **NEVER make arbitrary decisions.** When encountering ambiguity or inconsistency, ask the human.
- **NEVER read macro definitions as a first debugging step.** Read type/struct definitions in `.hpp` files instead. The `MAYBE` macro is intentional (like Rust's `?`), not technical debt.
- **Early development phase.** Prioritize functionality over polish.
- **Active focus:** Go generator (`ebm2go`) is the current development target.

## Build & Test Commands

```bash
# First-time setup (copies build_config.template.json, inits submodules)
python script/auto_setup.py

# Build everything
python script/build.py

# Build with explicit mode/type
python script/build.py native Debug
python script/build.py native Release

# Run all tests for a language generator
python script/unictest.py --target-runner ebm2go

# Run a single test by input name
python script/unictest.py --target-runner ebm2go --target-input simple_case

# Run tests with stdout visible (for debugging)
python script/unictest.py --target-runner ebm2go --print-stdout

# Generate EBM from .bgn source
./tool/ebmgen -i src/test/simple_case.bgn -o save/simple_case.ebm

# Generate target language code from EBM
./tool/ebm2go -i save/simple_case.ebm 1> save/go/simple_case.go

# Debug-print EBM to text
./tool/ebmgen -i save/simple_case.ebm -d save/debug.txt

# EBM interactive query
./tool/ebmgen -i save/simple_case.ebm --query "<id>" --query-format=text

# Create a new language generator skeleton
python script/ebmcodegen.py <lang_name>

# Create a new visitor hook template (class-based, preferred)
python script/ebmtemplate.py <HookName>_class <lang>

# Regenerate all EBM-related files after structure changes
python script/update_ebm.py
```

**Build requirements:** CMake >= 3.25, Clang++ with C++23 support, Ninja, Python 3.x.

## Project Architecture

Pipeline: `.bgn` -> `ebmgen` (AST->EBM IR) -> `ebm2<lang>` (EBM->code)

| Directory | Purpose |
|-----------|---------|
| `src/ebm/` | EBM IR format definition (auto-generated from `.bgn`) |
| `src/ebmgen/` | AST-to-EBM converter and interactive debugger |
| `src/ebmcodegen/` | Meta-generator framework (generates `ebm2<lang>` skeletons) |
| `src/ebmcg/ebm2<lang>/` | Compiled language code generators |
| `src/ebmcg/ebm2<lang>/visitor/` | **Where you write code** -- hook implementations |
| `src/ebmip/` | Interpreted/runtime language generators |
| `tool/` | Built executables (gitignored) |
| `save/` | Test output and temp files (gitignored) |

## Code Style

### Formatting (enforced by `.clang-format`)
- 4-space indentation, no tabs
- No column limit
- Google style base, heavily customized
- Opening braces on same line; `else` on new line after `}`
- Left-aligned pointers: `int* ptr`
- Includes are NOT auto-sorted

### Naming
- `snake_case` — variables, functions, fields
- `PascalCase` — types, structs, classes
- `UPPER_SNAKE_CASE` — macros, enum values
- Namespaces: `ebmgen`, `ebm`, `ebmcodegen`, `ebm2<lang>`

### Error Handling
The `MAYBE` macro is the primary error propagation mechanism. It works like Rust's `?` operator:
```cpp
// Evaluates expr; returns early on error; binds result to `name`
MAYBE(name, some_fallible_expression);

// Void variant (no result binding)
MAYBE_VOID(name, some_fallible_expression);
```
Do NOT replace `MAYBE` with try/catch or manual if-checks. This is a deliberate design choice.

### Hook Implementation Pattern
Visitor hooks use the class-based system (files named `*_class.hpp`):
```cpp
#include "../codegen.hpp"
DEFINE_VISITOR(HookName) {
    // ctx provides typed access to all EBM fields
    auto name = ctx.identifier();
    MAYBE(base, ctx.visit(ctx.base));
    MAYBE(field, ctx.get_field<"body.id.field_decl">(ctx.member));
    // Return CodeWriter or CODE(...) for output; return `pass` to fall through
    return CODE(base.to_writer(), ".", name);
}
```

Key APIs in hooks:
- `ctx.visit(ref)` — recursively visit a child node
- `ctx.get_field<"path.to.field">(ref)` — navigate EBM structure
- `ctx.identifier(ref)` — get the identifier string for a node
- `ctx.config()` — access the language-specific `Visitor` configuration
- `CODE(...)` — construct output code fragments
- `CODELINE(...)` — same but with a trailing newline
- Return `pass` from a before/after hook to fall through to main logic

### Hook Priority (lower wins)
- Priority 0: Language-specific `visitor/<Hook>_class.hpp`
- Priority 1: Language-specific `visitor/<Hook>.hpp` (legacy)
- Priority 4: `default_codegen_visitor/visitor/<Hook>_class.hpp`
- Priority 5: `default_codegen_visitor/visitor/<Hook>.hpp` (legacy)
- Before/after hooks (`_before_class.hpp`, `_after_class.hpp`) can hijack by returning a value instead of `pass`.

### Configuration via std::function Hooks
Language-specific behavior is configured in `entry_before_class.hpp` by setting `std::function` fields on `ctx.config()`:
```cpp
ctx.config().some_visitor = [&](Context_SomeType& sctx) -> expected<Result> {
    // custom logic
    return CODE("...");
};
```

## What to Edit vs What Not to Edit

**Edit these files:**
- `src/ebmcg/ebm2<lang>/visitor/*_class.hpp` — language-specific hooks
- `src/ebmcg/ebm2<lang>/visitor/includes.hpp` — shared helpers for a language
- `src/ebmcg/ebm2<lang>/config.json` — language configuration
- `src/ebmcodegen/default_codegen_visitor/visitor/*_class.hpp` — default hooks
- `src/ebmcodegen/default_codegen_visitor/visitor/Visitor.hpp` — default config fields
- Core framework files in `src/ebmgen/`, `src/ebmcodegen/`

**Never edit auto-generated files** (see Critical Warnings above).

## Development Workflow

1. Identify which EBM nodes need work (run tests, read errors)
2. Create hook templates: `python script/ebmtemplate.py <Hook>_class <lang>`
3. Implement logic in `visitor/*_class.hpp`
4. Rebuild: `python script/build.py`
5. Test: `python script/unictest.py --target-runner ebm2<lang>`
6. Debug: use `--print-stdout` or examine generated code in `save/<lang>/`

## Debugging Tips

- When encountering compile errors in generated code, examine the EBM structure using the interactive query: `./tool/ebmgen -i <file>.ebm --query "<id>" --query-format=text`
- Read type definitions in `extended_binary_module.hpp` to understand EBM node structure
- Check `src/ebmgen/GEMINI.md` for comprehensive development guidelines on the ebmgen component
- Debug output from tools is designed for core developers — do not over-rely on it
