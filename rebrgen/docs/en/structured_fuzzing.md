# Structured Fuzzing with ebm2rmw

ebm2rmw includes a structured fuzzing engine that leverages EBM format definitions to generate and mutate binary inputs. Unlike traditional byte-level fuzzers, this engine understands the field structure of the target format, producing inputs that exercise the encoder/decoder at a semantic level.

## Overview

The fuzzer operates in three modes:

| Mode | Flag | Description |
|------|------|-------------|
| **Generate** | `--fuzz-generate` | Create random inputs from scratch using the format spec |
| **Mutate** | `--fuzz-mutate` | Decode an existing binary, randomly mutate fields, re-encode |
| **Dictionary** | `--fuzz-dict FILE` | Export a fuzzer dictionary (AFL/libFuzzer format) |

All modes share these common flags:

| Flag | Default | Description |
|------|---------|-------------|
| `--fuzz-count N` | 1 | Number of inputs to produce |
| `--fuzz-seed SEED` | 0 (auto) | PRNG seed (0 = time-based; printed to stderr for replay) |
| `--fuzz-corpus-dir DIR` | (none) | Output directory for corpus files (`id_NNNNNN_seed_SEED.bin`) |

## Generate Mode

```bash
ebm2rmw -i format.ebm --entry-point StructName --fuzz-generate \
  --fuzz-count 100 --fuzz-seed 42 --fuzz-corpus-dir corpus/
```

### How it works

1. **Layout analysis**: The struct layout is resolved from the EBM type system — field offsets, sizes, and types are computed once.

2. **Zero-init + type-aware fill**: A fresh byte buffer is allocated (struct size) and filled recursively by `fuzz_fill_object`, which dispatches on `TypeKind`:

   - **INT/UINT**: Random value bounded to field bit-width. ~12% chance of an *interesting boundary value* (0, 1, 0x7f, 0x80, 0xff, 0xffff, MAX_INT, etc.).
   - **BOOL**: Random 0 or 1.
   - **ENUM/FLOAT**: Raw random bytes (the encoder handles semantic constraints).
   - **ARRAY**: Each element filled recursively.
   - **VECTOR**: Random length (geometric distribution biased toward small values, bounded by `--fuzz-max-vector-len`), elements allocated via `vector_alloc_back` and filled recursively.
   - **STRUCT**: Recursive field-by-field fill via `fuzz_fill_struct`.
   - **VARIANT/STRUCT_UNION**: Random variant selected and filled.

3. **Two-pass STRUCT_UNION handling** (`fuzz_fill_struct`):
   - **Pass 1**: Fill all non-STRUCT_UNION fields first. This populates discriminant/selector fields with random values.
   - **Pass 2**: For each STRUCT_UNION field, evaluate the compiled selector function against the now-populated parent struct to determine which variant is active, then fill only that variant.

   This ensures the discriminant value and the union variant are consistent — the encoder won't reject the input due to selector mismatch.

4. **Encode**: The filled object is passed through the format's compiled encode function, producing a structurally valid binary output. The encode function handles field dependencies (checksums, length fields, endianness) automatically.

5. **Output**: Written to `--fuzz-corpus-dir` (one file per input), or `--output-file` (single input), or hex-dumped via `--dump-output`.

### Key design decision

The encoder is the source of structural validity. Random fill + encode = valid binary. This avoids reimplementing format constraints in the fuzzer — the encode function already knows how to produce correct output from any field values.

## Mutate Mode

```bash
ebm2rmw -i format.ebm --entry-point StructName --fuzz-mutate \
  --binary-file seed_input.bin --fuzz-count 50 --fuzz-corpus-dir corpus/
```

### How it works

1. **Decode**: The seed binary is decoded into the runtime's object representation (byte buffer + type info).

2. **Collect mutable fields**: `collect_mutable_fields` recursively walks the struct layout and collects all leaf scalar fields (INT, UINT, BOOL, ENUM, FLOAT) with their byte offsets and sizes. Arrays are expanded element-by-element. Vectors and unions are skipped (their storage is arena-based, not contiguous in the decoded buffer).

3. **Per-iteration mutation**: For each output, the decoded bytes are copied, then `--fuzz-mutations N` random mutations are applied:

   | Strategy | Description |
   |----------|-------------|
   | **Bit flip** | Flip a single random bit in the field |
   | **Boundary value** | Replace with an interesting value (uses the same boundary table as generate mode) |
   | **Random replace** | Replace all bytes with random data |
   | **Byte swap** | Swap two random bytes within the field |
   | **Increment** | Add a small delta (-2..+2) to the field value |

   Each mutation picks a random field and a random strategy independently.

4. **Encode + output**: Same as generate mode.

### Additional flag

| Flag | Default | Description |
|------|---------|-------------|
| `--fuzz-mutations N` | 3 | Number of field mutations per output |

## Dictionary Generation

```bash
ebm2rmw -i format.ebm --entry-point StructName --fuzz-dict output.dict
```

Produces an AFL/libFuzzer-compatible dictionary file containing:

- **Enum member values**: Each enum constant emitted in both little-endian and big-endian byte order, sized to the enum's base type.
- **Boundary values per field size**: For each distinct scalar field size found in the struct (1, 2, 4, 8 bytes), emits zero, max-unsigned, and sign-bit-set tokens.

Example output:
```
# NOERROR_le
"\x00"
# SERVFAIL_le
"\x02"
# Boundary values by field size
# zeros_2
"\x00\x00"
# max_2
"\xff\xff"
# mid_2_le
"\x00\x80"
```

## Integration with External Fuzzers

ebm2rmw's normal mode (decode → modify → encode) already serves as a roundtrip harness. To use it with AFL++ or libFuzzer:

```bash
# AFL++ example
afl-fuzz -i corpus/ -o findings/ -x format.dict -- \
  ebm2rmw -i format.ebm --entry-point StructName --binary-file @@

# Generate initial corpus + dictionary
ebm2rmw -i format.ebm --entry-point StructName \
  --fuzz-generate --fuzz-count 100 --fuzz-corpus-dir corpus/
ebm2rmw -i format.ebm --entry-point StructName \
  --fuzz-dict format.dict
```

## Architecture

All fuzzing code lives in two files:

- **`visitor/fuzz.hpp`**: Self-contained fuzzing primitives
  - `FuzzRng` — mt19937_64 wrapper with boundary-value bias
  - `fuzz_fill_object` / `fuzz_fill_struct` — type-aware recursive fill
  - `collect_mutable_fields` — struct layout walker for mutation targets
  - `MutationKind` / `apply_mutation` — mutation strategies
  - `generate_fuzz_dict` — dictionary file generator
  - `resolve_fuzz_seed` / `prepare_corpus_dir` — shared setup helpers

- **`visitor/Statement_PROGRAM_DECL_class.hpp`**: Orchestration
  - `run_encode` — encode to buffer (no file I/O)
  - `write_corpus_file` — output routing (corpus dir / output file / hex dump)
  - `fuzz_generate_loop` — generate mode main loop
  - `fuzz_mutate_loop` — mutate mode main loop
