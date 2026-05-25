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
  and is stored into a `std::function<...>` field that outlives the lambda's
  enclosing scope. Today this currently happens to be safe in rebrgen because
  `dispatch_entry` keeps `before_ctx` alive throughout `main_logic()`. The
  invariant is temporal and fragile.

## Setup (one-time)

CodeQL CLI is not installed by default. Install:

```bash
# Linux / macOS:
gh extension install github/gh-codeql
gh codeql --version
# or download a release: https://github.com/github/codeql-cli-binaries/releases
```

Then fetch the standard C++ query library:

```bash
# inside this directory
codeql pack install
```

## Running queries

```bash
# from rebrgen/ root, build a CodeQL database (re-create whenever you re-build)
codeql database create save/codeql-db \
  --language=cpp \
  --command="python script/build.py" \
  --overwrite

# run a single query
codeql database analyze save/codeql-db \
  codeql-queries/escaped-ref-capture.ql \
  --format=sarif-latest --output=save/codeql-results.sarif

# pretty-print results
codeql bqrs decode save/codeql-db/results/*/escaped-ref-capture.bqrs --format=text
```

Database creation takes as long as a full build (~10 min). Subsequent query
runs against the same DB are seconds.

## Iterating on a query

CodeQL queries written without running them often need small fixes (predicate
name mismatches, type filter too tight, etc.). Iteration loop:

1. Edit `.ql` file
2. Re-run `codeql database analyze` (no DB rebuild needed)
3. Inspect SARIF for findings — count + sample locations
4. Adjust until findings match intent

If a query has zero hits on a codebase where you expect hits, the predicate
filter is usually too strict. If it has many false positives, narrow the
class definitions.

## Files

| File                         | Purpose                                                                   |
| ---------------------------- | ------------------------------------------------------------------------- |
| `qlpack.yml`                 | Declares this as a CodeQL pack depending on `codeql/cpp-all`              |
| `escaped-ref-capture.ql`     | Detects `[&]` capture of auto-storage local stored into `std::function`   |

## Adding a new query

1. Drop a new `<name>.ql` here with a `@kind problem` (or `@kind path-problem`) header
2. Document the trigger pattern in this README's "Files" table
3. Iterate locally until findings match intent — do not auto-wire into CI
