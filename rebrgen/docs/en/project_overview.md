## 1. Project Overview

### 1.1 Project Goal & Purpose
The `rebrgen` project is a generator construction project for `brgen` (https://github.com/on-keyday/brgen). While the original `brgen` code generator uses an AST-to-Code model, `rebrgen` adopts an AST-to-IR-to-Code model for improved flexibility and inter-language compatibility.
This subproject, `ebmgen`, is part of the IR project and is the successor to `bmgen` and `BinaryModule(bm)`. It converts the `brgen` Abstract Syntax Tree (AST) into a superior Intermediate Representation (IR) called the **`ExtendedBinaryModule` (EBM)**.
The `ebmcodegen` subproject is a code-generator-generator, which scans the `ExtendedBinaryModule` with a reflection mechanism based on a visitor pattern and generates C++ code that forms the backbone of code generators.

### 1.2 Workflow Overview
The overall workflow involves:
1.  A `.bgn` file is processed by `src2json` (from the `brgen` repository) to produce a `brgen` AST (JSON).
2.  The `brgen` AST (JSON) is input to `ebmgen`, which generates the EBM.
3.  The EBM is then processed by a language-specific code generator (built using `ebmcodegen`) to produce code in various languages.
4. If the `.bgn` file is `extended_binary_module.bgn` itself, `json2cpp2` (from the `brgen` repository) generates `extended_binary_module.hpp`/`.cpp` files, which are integrated with `ebmcodegen` to generate the C++ source code for the code generator.

### 1.3 The EBM: A Superior Intermediate Representation
The EBM (`src/ebm/extended_binary_module.bgn`) features a graph-based structure with centralized data tables where objects (Statements, Expressions, Types, etc.) are referenced by unique IDs. It supports structured control flow (if, loop, match), higher-level abstraction (focusing on 'what' rather than 'how'), lowered statements for a bridge to lower-level representations, and preserves critical binary format details like endianness and bit size.
