## 9. Current Project Status (as of 2026-02)

This document captures a snapshot of the project's current state, architecture maturity, and known issues.
It is intended as a reference for AI assistants and developers joining the project.

### 9.1 Development Phase

The project is in **early-to-mid development**. The priority is functionality over code cleanliness.
The core pipeline (AST -> EBM IR -> target code) is operational, and active work focuses on
expanding language generator coverage and improving the shared default visitor layer.

### 9.2 Active Development Focus

Recent development (last ~15 commits on `main`) has been concentrated on the **Go language generator** (`ebm2go`):
- Variant handling and type casting support
- Composite field and setter/getter handling
- Bool-mapped function support
- READ_DATA / WRITE_DATA statement hooks

Prior to that, work was done on type casting in `ebm2c` and general EBM infrastructure improvements.

### 9.3 Language Generator Status

#### Understanding Hook Counts

The number of language-specific hook files does **NOT** directly correlate with implementation completeness.
The framework has a three-tier `__has_include` fallback chain:

1. **Language-specific override**: `visitor/<Hook>.hpp` — custom logic for that language
2. **DSL-generated override**: `visitor/dsl/<Hook>_dsl.hpp` — (future/optional)
3. **Default fallback**: `ebmcodegen/default_codegen_visitor/visitor/<Hook>.hpp` — shared implementation

The **default codegen visitor** layer provides **83 hook implementations** covering most Statement, Expression,
and Type visitors. These defaults are parameterized via a `Visitor` configuration struct (~100 fields:
`begin_block`, `end_block`, `use_brace_for_condition`, `function_define_keyword`, etc.) plus `std::function`
callbacks for language-specific extensions.

As a result:
- **Earlier generators** (Rust, C) were developed when the default layer was immature, requiring more
  language-specific overrides. Many of these could potentially be refactored into the default layer.
- **Later generators** (Go) benefit from a mature default layer and need fewer overrides.
- Hook count alone does not indicate completeness — a generator with fewer hooks may be equally functional
  if it relies on well-configured defaults.

#### Per-Generator Summary

| Generator | Specific Hooks | Default Inheritance | Notes |
|-----------|---------------|--------------------:|-------|
| **ebm2rust** | 45 | ~46% | Oldest. Heavy type-system overrides (UINT, STRUCT, ENUM, VARIANT, ARRAY, VECTOR, OPTIONAL, PTR, etc.). Could benefit from refactoring common patterns into defaults. |
| **ebm2rmw** | 33 | N/A (uses `default_interpret_visitor`) | Interpreter, not codegen. Fundamentally different approach — emits `Instruction` objects, not text. Has its own `Result.hpp`, `inst.hpp`, `interpret.hpp`. |
| **ebm2c** | 32 | ~61% | C's macro-based I/O system and header-only generation (`static inline`) require significant custom logic. |
| **ebm2python** | 24 | ~71% | Moderate customization. Has DSL sample files in `dsl_sample/`. |
| **ebm2p4** | 15 | ~82% | P4 networking language. Some hooks may still be unimplemented rather than inherited. |
| **ebm2go** | 12 | ~86% | Most recent. Lightest customization, benefiting most from default layer maturity. Actively being developed. |

#### How Language Generators Customize Behavior

Generators can customize at several levels (without writing full hook overrides):
1. **`Visitor_before.hpp`** — Set configuration fields on the default `Visitor` struct (syntax tokens, naming conventions, behavioral flags)
2. **`_before` / `_after` hook files** — Pre/post-processing around default logic, with optional "hijack" to override entirely
3. **`std::function` callbacks** — Set in Visitor config for targeted customization (e.g., `struct_definition_start_wrapper`, `write_data_visitor`)
4. **Full hook override** — Provide a complete `visitor/<Hook>_class.hpp` replacing the default

### 9.4 Core Framework Maturity

#### EBM IR (`src/ebm/`)
- Stable graph-based IR with 5 centralized tables (Identifiers, Strings, Types, Statements, Expressions)
- Varint-encoded references with alias/deduplication system
- Supports lowered IO statements for progressive lowering
- Property system for union types, composite fields, debug source locations
- Generated C++ implementation (`extended_binary_module.hpp/cpp`) plus zero-copy variants

#### ebmgen — AST-to-EBM Converter (`src/ebmgen/`)
- Operational pipeline: convert -> add_files -> 7 transform passes -> finalize
- Transform passes: flatten IO -> CFG + bit IO lowering -> merge bit fields -> vectorize IO -> derive properties -> add cast -> remove unused
- Interactive debugger with XPath-like query engine functional
- Known TODOs:
  - `expression.cpp`: Complex type conversions (ARRAY, VECTOR, STRUCT, RECURSIVE_STRUCT) not yet implemented
  - `expression.cpp`: Common type inference is "best effort"
  - `encode.cpp` / `decode.cpp`: Some argument handling incomplete
  - `statement.cpp`: Strict state variable analysis not yet implemented
  - `bit_holder.cpp`: Struct type mapping incomplete

#### ebmcodegen — Meta-Generator (`src/ebmcodegen/`)
- Two hook systems coexist: old (`#if __has_include`) and new (class-based with context types)
- Class-based system provides 7-layer hook priority, before/after with hijack, full IDE autocompletion
- `class_based.cpp` (2004 lines) is the largest single source file in the framework
- Default codegen visitor (83 hooks) provides substantial shared logic
- Known issue: `HasVisitor` concept doesn't work correctly in some compilers when CRTP is involved (macro workaround in place)

### 9.5 Build and CI Status

#### Build System
- CMake 3.25+ / Ninja / Clang with C++23
- Two modes: native (default) and web (Emscripten/WASM)
- Auto-generated `main.cpp` files are ~4MB each, `codegen.hpp` ~400KB each
- Bootstrap dependency: `update_ebm.py` requires `ebmcodegen` to regenerate files that `ebmcodegen` depends on (handled via `CODEGEN_ONLY=1` partial build)

#### CI (GitHub Actions)
- Three jobs: build-wasm, build-native, deploy
- **Only runs on Ubuntu** — no Windows CI despite Windows being a development platform
- Known minor issues:
  - Cache name mismatch (`cache-web-built` used in native job — copy-paste error, functionally harmless due to key prefix)
  - Duplicate `libc++` install step in native job
- Dependabot monitors GitHub Actions and npm versions

#### Testing
- `unictest.py` framework with 28 test `.bgn` files (dns_packet, http2_frame, websocket, varint, bgp, etc.)
- Each generator has `unictest.py` + `unictest_runner.json` for per-language testing
- Test verification: binary round-trip comparison (byte-level)
- No automated coverage tracking for which hooks are implemented per generator

### 9.6 Known Technical Debt and Risks

1. **Auto-generated file sizes**: ~4MB `main.cpp` and ~400KB `codegen.hpp` per generator impact build times and IDE performance. These are compile-time costs paid on every build.

2. **Compiler lock-in**: `build.py` hardcodes `clang++`. No GCC/MSVC support. The `HasVisitor` concept issue hints at potential portability problems.

3. **Bootstrap circularity**: Changes to `extended_binary_module.bgn` require a careful multi-step rebuild process (`update_ebm.py`). If the EBM format changes break `ebmcodegen` itself, manual intervention may be needed.

4. **Legacy system**: `src/old/` (bmgen, bm2c, bm2cpp, bm2go, bm2rust, bm2python, bm2haskell, bm2kaitai, bm2hexmap) still builds when `BUILD_OLD_BMGEN=true`. Not actively maintained but consumes build time.

5. **Missing Windows CI**: Development happens on Windows, but CI only tests Ubuntu. Platform-specific issues could go undetected.

6. **`auto_setup.py` directory bug**: `os.chdir()` calls don't restore the original working directory, which could cause subsequent script invocations to fail.

### 9.7 Repository Structure Quick Reference

```
brgen/                  Git submodule (parser, AST) — pinned at v0.0.7+213 commits
src/
  ebm/                  EBM IR definition (.bgn + generated C++)
  ebmgen/               AST-to-EBM converter + interactive debugger
    convert/            AST node conversion (statement, expression, type, encode, decode)
    transform/          IR optimization passes (7 passes)
    interactive/        Debugger and query engine
  ebmcodegen/           Meta-generator framework
    default_codegen_visitor/visitor/   83 shared hook implementations
    default_interpret_visitor/         Interpreter-mode defaults
    dsl/                DSL compilation
    stub/               Base visitor infrastructure
  ebmcg/                Compiled language generators
    ebm2c/              C generator (32 hooks)
    ebm2go/             Go generator (12 hooks, active development)
    ebm2p4/             P4 generator (15 hooks)
    ebm2python/         Python generator (24 hooks)
    ebm2rust/           Rust generator (45 hooks)
  ebmip/                Interpreted generators
    ebm2rmw/            RMW interpreter (33 hooks)
  old/                  Legacy bmgen system (inactive)
  test/                 28 test .bgn files
script/                 Build, generation, and testing scripts (27 files)
tool/                   Built executables (gitignored)
docs/                   Documentation (en/, ja/, gemini/, draft/, old/)
```

### 9.8 Branches

| Branch | Purpose |
|--------|---------|
| `main` | Primary development branch, up to date with `origin/main` |
| `ai/implement_rust` | AI-assisted Rust generator implementation |
| `codegen/dsl` | DSL-based code generation experiment |
| `fix-ebm-cast-conversion` | EBM cast conversion fix |
| `interpreter` | Interpreter mode development |
| `refactor/hook_system` | Hook system refactoring |
| `refactor/using_ai` | AI-assisted refactoring |
| `self_ptr` | Self-pointer feature |
