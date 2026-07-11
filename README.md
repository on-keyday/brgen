# brgen - BinaRy encoder/decoder GENerator

[日本語版 README はこちら](README.ja.md)

**brgen** is a binary format definition language and a suite of code generators. You describe network packets, file formats, and other binary structures once in the `.bgn` language, and brgen generates encoder/decoder code for multiple target languages.

Pronounced "B-R-Gen" (the author says "B-R-Gehn", romaji-style — not that anyone else has to).

## Quick Example

```
format UDPHeader:
    src_port :u16
    dst_port :u16
    length   :u16
    checksum :u16

format UDPDatagram:
    header :UDPHeader
    data :[header.length-8]u8
```

From this definition, brgen generates encode/decode functions in C++, Go, Rust, TypeScript, and more.

Try it now in the [Web Playground](https://on-keyday.github.io/brgen/) — no installation needed. Press F1 (or right-click → Command Palette) and type `load example file` to load sample definitions.

More than 100 real-world format definitions (UDP, TCP, DNS, TLS, ZIP, ELF, ...) are in [`example/`](https://github.com/on-keyday/brgen/tree/main/example).

## Goals

- **Enough to represent formats** — expressive enough for real-world network protocols and file formats
- **Easy to write and read**
- **Write once, generate any language**

The ultimate goal (a lofty ambition, admittedly) is to make it common practice to attach a brgen definition to binary format specifications. A spec with an unambiguous, machine-readable definition can be fed straight into a code generator — and even where generated code cannot be used, the unambiguous definition itself helps manual implementers and newcomers alike.

## Status

This product is under development. No guarantee is provided for the results of using it, and breaking changes may occur without notice.

Development follows an MVP-first approach: the repository contains many partially complete but functional components, and generators are not required to cover every feature of their target language. Per-generator conformance is continuously verified by [unictest (e2e tests)](https://github.com/on-keyday/brgen/tree/main/src/tool/unictest), and the results are published:

**https://on-keyday.github.io/brgen/unictest-results/**

Contributions to test cases, the test framework, and backend development are very welcome.

## Architecture

```
.bgn file → [src2json] → AST (JSON) ─┬→ [json2<lang>] → target code          (1st generation)
                                     └→ [ebmgen] → EBM IR → [ebm2<lang>]
                                                              → target code  (2nd generation)
```

brgen has two generations of code generators:

- **1st generation — AST-to-Code** (`src/tool/json2*`): generators that translate the AST directly. Targets include C++ (`json2cpp2`), C, Go, TypeScript, Rust, Kaitai Struct, Mermaid, and Graphviz. These power the Web Playground and the AST library generation. Each is written in its target's ecosystem language (C++/Go/Rust) as a proof that the AST can be consumed from any language.
- **2nd generation — AST-to-IR-to-Code** ([`rebrgen/`](rebrgen/)): the current main line of development. The AST is lowered to the Extended Binary Module (EBM) intermediate representation, and `ebm2<lang>` generators are built on a shared visitor framework. Targets: C, C++, C#, Go, Java, LLVM IR, P4, Python, Ruby, Rust, TypeScript, Wuffs, Z3, Zig — plus non-source backends (JSON dump, ASCII visualization, and an interpreter used for structured fuzzing). These are the generators verified by unictest above.

See [`rebrgen/README.md`](rebrgen/README.md) for the 2nd-generation architecture in detail.

## How Does brgen Differ from Similar Tools?

- **Kaitai Struct** — the closest neighbor: binary format definition → multi-language parser generation. Kaitai officially supports decoding only, while brgen generates both encoders and decoders. `.bgn` also lets you write control flow and expressions directly, and brgen publishes its IR (EBM) as an independent binary format.
- **Protocol Buffers / Thrift / Cap'n Proto** — IDLs for serialization formats you design yourself. They are not meant for describing existing binary protocols (a TCP header, a TLS record) — and describing existing formats is exactly brgen's primary use case.
- **Zeek Spicy** — the closest in approach (explicit IR pipeline: Spicy → HILTI → C++), but it targets C++ only and requires a runtime library. brgen targets many languages and keeps runtime dependencies of generated code minimal.
- **P4** — a packet-processing language for programmable switches/NICs: you define packet headers and parsers/deparsers and run them directly on the target. It shares with brgen the problem space of describing binary headers in a DSL, but it is an execution language rather than a multi-language code generator.

Each of these tools is successful in its own domain, and brgen does not claim to supersede them. The long-term direction is coexistence rather than competition: unify the format-definition entry point in `.bgn` while reusing each tool's ecosystem as a backend — `ebm2p4` (P4 output) is an existing example. See [`rebrgen/docs/decisions/0021-positioning-among-idl-tools.md`](rebrgen/docs/decisions/0021-positioning-among-idl-tools.md) (Japanese) for the full analysis.

## Getting Started

- **No install**: use the [Web Playground](https://on-keyday.github.io/brgen/).
- **Prebuilt binaries**: download from [GitHub Releases](https://github.com/on-keyday/brgen/releases).
- **Build from source**: requires CMake, Ninja, Clang++ (C++20), Go, and Python 3 (Emscripten/npm additionally for the web build).

```bash
python build.py native   # native tools → tool/
python build.py web      # WASM build for the playground
python build.py all      # native + wasm + npm + generate + lsp
```

To run the whole pipeline against `example/`, configure `brgen.json` (input/output directories, generators) and run `tool/brgen`.

## Documentation

- Documentation site: https://on-keyday.github.io/brgen/doc
- Format examples: [`example/`](https://github.com/on-keyday/brgen/tree/main/example)
- 2nd-generation generator framework: [`rebrgen/`](rebrgen/)

## Writing Your Own Generator

If your favorite language is missing, you can write a generator yourself:

- **Recommended**: use the rebrgen framework — it generates the visitor skeleton for a new `ebm2<lang>` backend, so you only implement language-specific hooks. See [`rebrgen/README.md`](rebrgen/README.md).
- **AST libraries** for consuming the parsed AST directly are provided for C++ (`src/core/ast/`), Go, TypeScript, Rust, and Python (`astlib/`).

Pull requests for new generators and AST libraries are welcome.

## Contributing

Bug reports, feature requests, and pull requests are welcome via [GitHub Issues](https://github.com/on-keyday/brgen/issues/new). See [CONTRIBUTING.md](CONTRIBUTING.md) for the issue/PR policy and branch naming conventions, and [SECURITY.md](SECURITY.md) for vulnerability reports.

## Acknowledgements

This project was started as a project of [SecHack365'23](https://sechack365.nict.go.jp/). The code at the time of the final presentation is tagged `SecHack365-final`.

## License

MIT License.

All source code is released under the MIT license. The copyright remains with on-keyday and contributors. Contributions must agree to be published under the MIT license.

Code generated by the generators (and not contained in this repository) may be licensed freely by you.

For prebuilt binaries on GitHub Releases, licenses of dependencies are collected by [licensed](https://github.com/github/licensed) and [gocredits](https://github.com/Songmu/gocredits) and bundled with the binaries. If you find a license problem, please tell us via GitHub Issue. See also [license_note.txt](script/license_note.txt).
