# RE:brgen

RE:brgen (rebrgen) is a code generator construction framework for [brgen](https://github.com/on-keyday/brgen), a binary format definition language.

While the original brgen code generators are designed as an AST-to-Code model, rebrgen implements an AST-to-IR-to-Code model using the Extended Binary Module (EBM) intermediate representation. This is aimed at improving flexibility and inter-language compatibility.

rebrgen was originally a separate repository, but is now integrated into the brgen monorepo as the `rebrgen/` subdirectory. Directory structure and build systems remain separate; see `docs/decisions/0015-rebrgen-integration.md`.

## Pipeline

```
.bgn file → [src2json] → brgen AST (JSON) → [ebmgen] → EBM → [ebm2<lang>] → target code
```

- **EBM** (`src/ebm/`) — a graph-based IR with centralized tables for statements, expressions, and types, referenced by IDs. The format itself is defined in brgen DSL (`extended_binary_module.bgn`).
- **ebmgen** (`src/ebmgen/`) — converts brgen AST (or `.bgn` directly, via libs2j) to EBM, with IR transform passes and an interactive query engine.
- **ebmcodegen** (`src/ebmcodegen/`) — a meta-generator that produces the visitor-based skeleton for each `ebm2<lang>` tool. Developers implement only language-specific visitor hooks.
- **ebm2\<lang\>** — the generated code generators:
  - `src/ebmcg/` (target source code generators): C, C++, C#, Go, Java, LLVM IR, P4, Python, Ruby, Rust, TypeScript, Wuffs, Z3, Zig
  - `src/ebmip/` (non-source-code backends): `ebm2json` (serialization), `ebm2ascii` (visualization), `ebm2rmw` (interpreter, used for structured fuzzing)

Maturity varies per language; see the [status page](https://on-keyday.github.io/brgen/doc/docs/rebrgen/status/) and the unictest results below.

## How to Build

Requires CMake >= 3.25, Ninja, Clang++ (C++23), and Python 3.

1. Copy `build_config.template.json` to `build_config.json` (done automatically by `auto_setup.py` if missing).
2. To also build brgen tools / the futils dependency, set `AUTO_SETUP_BRGEN` / `AUTO_SETUP_FUTILS` to `true` in `build_config.json`.
3. First time: `python script/auto_setup.py`. Subsequent builds: `python script/build.py`.

Built executables go to `tool/`.

## Quick Usage

```bash
# .bgn → EBM (recommended; requires libs2j)
./tool/ebmgen -i format.bgn -o format.ebm

# or from a brgen AST JSON (e.g. produced by src2json or the Web Playground)
./tool/ebmgen -i format.json -o format.ebm

# EBM → target language code
./tool/ebm2rust -i format.ebm

# Debug print EBM as human-readable text
./tool/ebmgen -i format.ebm -d format.txt

# Interactive query engine for inspecting EBM
./tool/ebmgen -i format.ebm --interactive
```

You can write `.bgn` files with the [Web Playground](https://on-keyday.github.io/brgen/).

## How to Add a Language

1. Make sure `tool/ebmcodegen[.exe]` is built.
2. Run `python script/ebmcodegen.py <lang>` to create the `src/ebmcg/ebm2<lang>/` skeleton.
3. Run `python script/build.py` again to build `tool/ebm2<lang>`.
4. Run `python script/unictest.py --target-runner ebm2<lang>` to discover unimplemented visitor hooks.
5. Generate hook templates with `python script/ebmtemplate.py <hook>_class <lang>` and implement them in `src/ebmcg/ebm2<lang>/visitor/`. Run `python script/ebmtemplate.py interactive` for guided mode.
6. Iterate on step 4 until tests pass.

## Testing

Automated end-to-end tests (encode/decode round-trips over the `.bgn` files in `src/test/`) are run with unictest:

```bash
python script/unictest.py --target-runner ebm2<lang>              # one language
python script/unictest.py --target-runner ebm2<lang> --print-stdout  # verbose
```

## Documentation

- [Published docs](https://on-keyday.github.io/brgen/doc/docs/rebrgen/) — overview, tool reference, visitor hooks, testing, status (source: `../web/doc/content/docs/rebrgen/`)
- `docs/en/`, `docs/ja/` — in-repo documentation (partially AI-generated; may lag behind the code)
- `docs/decisions/` — Architecture Decision Records

## Legacy

`src/old/` contains the previous bmgen/bm2 system. It is no longer actively developed; ebmgen/ebmcodegen supersede it.

## LICENSE

MIT License
