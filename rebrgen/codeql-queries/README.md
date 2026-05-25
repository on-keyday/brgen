# rebrgen CodeQL custom queries

Custom CodeQL queries targeting **rebrgen-specific** patterns that aren't caught
by the default rule packs and can't be expressed in the C++ type system.

**This directory is opt-in / local-only.** It is intentionally NOT wired into
CI — the previous CI-wired CodeQL workflow was removed (commit `7a0feaa1`)
because its default-pack analysis produced near-zero useful findings on this
codebase. Queries here are meant to be run on demand when you suspect a
specific structural bug pattern.

## Why this exists (instead of regex / linter scripts)

These patterns require either inter-procedural data flow or AST-level structural
analysis that a regex linter (`script/check_*.py`) can't express:

- **Capture lifetime escape**: a lambda `[&]`-captures an auto-storage local
  and is assigned (via `std::function::operator=`) to a config field whose
  lifetime outlives the lambda's enclosing scope. Today this currently happens
  to be safe in rebrgen because `dispatch_entry` keeps `before_ctx` alive
  throughout `main_logic()`. The invariant is temporal and fragile.

## Setup (one-time)

CodeQL CLI 2.24.x or later. A windows-x64 build was placed in `save/codeql/`
for ad-hoc use:

```bash
save/codeql/codeql.exe --version    # → CodeQL command-line toolchain release 2.24.3.
```

If you don't have it locally, download from
<https://github.com/github/codeql-cli-binaries/releases> and unzip into `save/codeql/`.

Then fetch the C++ standard query library (writes `codeql-pack.lock.yml`):

```bash
save/codeql/codeql.exe pack install codeql-queries
```

## Building the CodeQL database

The C++ extractor traces actual compiler invocations, so the build command must
re-compile at least the translation units you care about. For most uses,
**selectively clean one ebm2X target** so the existing incremental build state
is preserved:

```bash
# example: focus on ebm2zig
rm built/native/Debug/src/ebmcg/ebm2zig/CMakeFiles/ebm2zig.dir/main.cpp.obj

save/codeql/codeql.exe database create save/codeql-db \
  --language=cpp \
  --command="python script/build.py" \
  --overwrite
```

DB size with a single TU under trace: ~80 MB. Build time: ~1–2 min (PCH cached).

For a full-scope DB across all `ebm2X` languages, delete all the corresponding
`*.cpp.obj` files first. Expect ~3–5 GB DB and ~15–30 min build time.

## Running queries

```bash
# strict (zero-FP) query — bug-detector mode
save/codeql/codeql.exe database analyze save/codeql-db \
  codeql-queries/escaped-ref-capture.ql \
  --format=sarif-latest --output=save/codeql-results.sarif --rerun

# loose query — review-aid mode (will surface "scary-but-safe" captures too)
save/codeql/codeql.exe database analyze save/codeql-db \
  codeql-queries/escaped-ref-capture-review.ql \
  --format=sarif-latest --output=save/codeql-review.sarif --rerun
```

Pretty-print findings:

```bash
python -c "
import json, sys
d = json.load(open(sys.argv[1]))
for run in d['runs']:
    for r in run.get('results', []):
        loc = r['locations'][0]['physicalLocation']
        uri = loc['artifactLocation']['uri']
        line = loc.get('region', {}).get('startLine', '?')
        print(f'{uri}:{line}  {r[\"message\"][\"text\"]}')
" save/codeql-results.sarif
```

## Empirical baseline (taken from ebm2zig on 2026-05-26)

| Query                              | Hits | What it means                                          |
| ---------------------------------- | ---- | ------------------------------------------------------ |
| `escaped-ref-capture.ql`           | 0    | No truly dangerous escapes in current code.            |
| `escaped-ref-capture-review.ql`    | 13   | All `[&]ctx` patterns — safe today via dispatch_entry. |

If the strict query starts firing in the future, that's a real bug. If the
review-aid count changes, audit what changed — new patterns may break the
dispatch_entry lifetime invariant.

## Iterating on a query

CodeQL queries written without running them often need small fixes. Real traps
hit during this directory's initial write:

1. **`config.X = lambda` is NOT an `AssignExpr`** when `X` is `std::function`.
   It's parsed as a `FunctionCall` to `std::function::operator=`. Match
   `FunctionCall` with `getTarget().hasName("operator=")`, not `AssignExpr`.
2. **`LambdaCapture.getField()` returns the synthesized closure field**, which
   has no `.getVariable()`. The captured-source variable is reached via
   `.getInitializer().(VariableAccess).getTarget()`.
3. **Field type representation**: `std::function<T(U)>` shows up as
   `function<..(..)>` in `getUnspecifiedType().toString()`. Match
   `function<%>`, not `std::function<%>`.

Iteration loop:

1. Edit `.ql` file
2. Re-run `codeql database analyze ... --rerun` (DB cached, no rebuild)
3. Inspect SARIF
4. Adjust class/predicate filters until findings match intent

`--rerun` is important: without it, codeql reuses the previous query's cached
results.

## Files

| File                              | Purpose                                                              |
| --------------------------------- | -------------------------------------------------------------------- |
| `qlpack.yml`                      | Declares this as a CodeQL pack depending on `codeql/cpp-all`         |
| `codeql-pack.lock.yml`            | Pinned dependency versions (generated by `codeql pack install`)      |
| `escaped-ref-capture.ql`          | Strict: definitely-escaped auto-storage locals (0 hits on ebm2zig)   |
| `escaped-ref-capture-review.ql`   | Loose: includes reference captures, for human lifetime audit         |

## Adding a new query

1. Drop a new `<name>.ql` here with proper `@kind` / `@id` / `@problem.severity` header
2. Document the trigger pattern in this README's "Files" table
3. Iterate locally until findings match intent — do not auto-wire into CI
