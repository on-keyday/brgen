# Project Terminology (v7 - Restored Details)

This document defines key terms related to the `brgen` and `rebrgen` projects.

## 1. The `brgen` Project: Motivation and Goals

Writing the logic to serialize and deserialize binary formats, such as those used in network protocols, is a tedious and error-prone task. A significant challenge is the lack of a standardized notation for defining these formats, which creates a high barrier to entry for beginners and individual developers. 

The `brgen` project was created to solve this problem. It is a format definition language and a code generator development project with three primary goals:

1.  **Enough to represent formats**: The language should be expressive enough to define a wide variety of complex binary formats.
2.  **Easy to write and read**: The syntax should be intuitive and clear, simplifying the process of defining formats.
3.  **Write once, generate any language code**: From a single format definition, it should be possible to generate code in multiple programming languages.

---

## 2. `rebrgen`: Improving the Generator Architecture

While `brgen` is a powerful tool, its original direct AST-to-Code generation approach made it difficult to ensure consistent behavior (semantics) across different target languages. It also increased the effort required to port the generator to a new language. 

This project, `rebrgen`, was created to address these challenges. It introduces a new layer, an Intermediate Representation (IR) named EBM, between the initial parsing of the `.bgn` file and the final code generation. This new AST-to-IR-to-Code model aims to improve the ease of code generator development and reduce semantic differences between language outputs.

- **brgen**: The original project that provides a binary encoder/decoder generator. It uses a direct AST-to-Code approach.
- **rebrgen**: This project. A framework for building code generators that uses a more modular AST-to-IR-to-Code model, with EBM as the IR.
- **.bgn**: A custom Domain-Specific Language (DSL) used by `brgen` to define binary data formats.

---

## 3. Fundamental Concepts (For Beginners)

- **Code Generator**
  - **Formal Definition**: A program that takes structured data as input and produces source code in a specific programming language as output. 
  - **In Simple Terms**: Think of it as a translator that, instead of translating from Japanese to English, translates a structured blueprint (`.bgn` file) into a functional program in a language like C++ or Python.

- **AST (Abstract Syntax Tree)**
  - **Formal Definition**: A tree representation of the abstract syntactic structure of source code.
  - **In Simple Terms**: Imagine diagramming a sentence. The AST is like that diagram for code, breaking it down into its fundamental parts (operations, values, etc.) and showing how they are connected to each other. It represents the code's structure, ignoring things like commas or parentheses.

- **DSL (Domain-Specific Language)**
  - **Formal Definition**: A computer language specialized for a particular application domain.
  - **In Simple Terms**: A DSL is a mini-language designed to do one job really well. Instead of a general-purpose language like Python that can do anything, a DSL like `.bgn` is specifically designed for the single task of describing binary data layouts.

- **IR (Intermediate Representation)**
  - **Formal Definition**: A data structure that represents code in a way that is independent of both the source and target languages. It serves as a middle layer between a parser and a code generator.
  - **In Simple Terms**: An IR is like a universal language for code. The parser first translates the source `.bgn` file into this universal language (the EBM). Then, each code generator translates from this universal language into its specific target language (C++, Python, etc.). This is easier than creating a direct translator from `.bgn` to every single possible language.

---

## 4. For Language Generator Developers

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

## 5. For Core Tooling Developers

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

- **src2json**: An artifact of the original `brgen` project, this tool (or its internal functionality) is used to parse `.bgn` files and output the `brgen` AST in JSON format.
- **json2cpp2**: An artifact of the original `brgen` project, this tool is used to generate C++ files from a JSON definition. In `rebrgen`, it is used to generate the C++ representation of the EBM structure itself from `extended_binary_module.bgn`.
- **update_ebm.py**: A script that automates the full process of regenerating all core files when the EBM structure (`extended_binary_module.bgn`) is changed.
