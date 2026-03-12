# Go Generator Improvement

## Goal

Make the Go language generator tests pass.

## Plan

1. Run the `script/run_cmptest` to see the current status of the tests.
2. Examine the `save/go/save.go` file to understand the syntax errors.
3. Examine the code generator `src/old/bm2go/bm2go.cpp` to understand how the code is generated and identify the source of the syntax errors.
4. Identify the source of the missing curly braces in the generated code.
5. Fix the code generator to generate valid Go code.
6. Run the `script/run_cmptest` to see if the tests pass.

## Actions

1. Run `script/run_cmptest.ps1` and observe the failing tests.
2. Read the contents of `save/go/save.go` and identify the syntax errors.
3. Read the contents of `src/old/bm2go/bm2go.cpp` and analyze the code generation logic.
