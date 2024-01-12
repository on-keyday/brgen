---
title: "Generator Dev"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# Generator の開発

ジェネレーターの開発に関するメモ

## 基本的な流れ

以下擬似コード

```py
import ast2py as ast

def generate_root(prog :ast.Program):
    for element in prog.elements:
        if isinstance(element,ast.Format):
            generate_format(element)

def generate_format(fmt :ast.Format)
    generate_definition(fmt.struct_type)
    if fmt.encode_fn is not None:
        generate_encoder(fmt.encode_fn.body)
    else:
        generate_encoder(fmt.body)

    if fmt.decode_fn is not None:
        generate_decoder(fmt.decode_fn.body)
    else:
        generate_decoder(fmt.body)

def generate_definition(block :ast.IndentBlock):
```
