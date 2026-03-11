# Project Terminology (v4 - Reorganized)

This document defines key terms related to the `rebrgen` project, organized by audience.

## 1. Fundamental Concepts (For Beginners)

- **Code Generator**: A program that takes structured data as input and produces source code in a specific programming language as output. In this project, it refers to a program that generates C++, Rust, Python, etc., code from an Intermediate Representation (EBM).
- **AST (Abstract Syntax Tree)**: A tree representation of the abstract syntactic structure of source code. The `brgen` project first parses `.bgn` files into an AST.
- **DSL (Domain-Specific Language)**: A computer language specialized for a particular application domain. In this project, `.bgn` is a DSL for defining binary data formats.
- **IR (Intermediate Representation)**: A data structure used to represent source code between the front-end (parser) and the back-end (code generator). It is an abstraction that is independent of both the original source language and the target language. This project's primary IR is the EBM.

---

## 2. Project Core Concepts

This project, `rebrgen`, was created to address challenges found in the original `brgen` project. While `brgen` is a powerful tool, its direct AST-to-Code approach made it difficult to ensure consistent behavior (semantics) across different target languages and increased the effort required to port the generator to a new language. `rebrgen` introduces a new layer, an Intermediate Representation (IR) named EBM, between the initial parsing of the `.bgn` file and the final code generation. This new AST-to-IR-to-Code model aims to improve the ease of code generator development and reduce semantic differences between language outputs.

- **brgen**: The original project that provides a binary encoder/decoder generator based on a custom DSL (`.bgn` files). It uses a direct AST-to-Code approach.
- **rebrgen**: This project. A framework for building code generators that uses a more modular AST-to-IR-to-Code model. The IR used is the EBM.
- **.bgn**: A custom Domain-Specific Language (DSL) used by `brgen` to define binary data formats.

---

## 3. For Language Generator Developers

This section is for developers who use the `rebrgen` framework to create a code generator for a specific target language (e.g., `ebm2rust`).

### Workflow & Core Tools

- **ebmcodegen**: A tool that generates the C++ boilerplate/skeleton for a new language-specific code generator (e.g., `ebm2<your_lang>`).
- **visitor hook**: A C++ code snippet (`.hpp` file) where you implement the language-specific code generation logic for a specific part of the EBM (e.g., how to generate code for an `if` statement). These are the primary files you will create and edit.
- **ebmtemplate.py**: A helper script used to create and manage `visitor hook` files.
- **unictest.py**: The primary script for testing your generator. It automates the process of running `ebmgen`, running your generator, and executing tests.

### C++ Development

- **MAYBE macro**: A C++ macro you will encounter and use within visitor hooks for error handling. It checks for an error state and performs an early return, similar to Rust's `?` operator.
- **get_associated_identifier**: A function available in visitor hooks to get the string name for an identifier. This is the recommended way to get names for variables, functions, etc.

---

## 4. For Core Tooling Developers

This section is for developers working on the internal components of `rebrgen` itself, such as `ebmgen`.

### EBM (Extended Binary Module)

- **EBM (ExtendedBinaryModule)**: The central Intermediate Representation (IR) of the project. It features a graph-based structure where objects are stored in centralized tables and linked by references (`...Ref` types). Its design separates it from the previous "bm" (Binary Module) IR.
- **...Ref types (e.g., `StatementRef`, `TypeRef`)**: Reference types that act as wrappers around a unique ID. They are used throughout the EBM to link to objects, forming the graph structure.

### ebmgen (AST-to-EBM Converter)

- **ebmgen**: The tool that converts the `brgen` AST (from a `.bgn` file) into the EBM.
- **ConverterContext**: The central object that orchestrates the AST-to-EBM conversion process. It holds the state and repositories.
- **EBMRepository**: A component that builds the EBM. It manages the creation of all EBM objects, assigns unique IDs, and uses `ReferenceRepository` instances to store them and prevent duplication.
- **ReferenceRepository**: A template class that manages a collection of a specific EBM object type (e.g., all `Statement` objects), handling ID assignment and caching.
- **ConverterState**: A struct holding the transient state during conversion (e.g., current endianness, visited AST nodes).
- **MappingTable**: A class used after the EBM is built to provide fast, convenient access to its objects via hashmaps. It is used by code generators and the `ebmgen` debugger.

### Utility Scripts & Tools

- **src2json**: A tool from the `brgen` repository that parses `.bgn` files and outputs the `brgen` AST in JSON format. `ebmgen` uses this functionality internally.
- **json2cpp2**: A tool from the `brgen` repository that generates C++ files from a JSON definition. Used to generate the C++ representation of the EBM itself.
- **update_ebm.py**: A script that automates the full process of regenerating all core files when the EBM structure (`extended_binary_module.bgn`) is changed.
