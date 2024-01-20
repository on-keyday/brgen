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

ジェネレーターは必ずしも brgen(lang)の全機能に対応している必要はない。
このリポジトリ付属のジェネレーターはひととおりできるよう対応していくつもりではあるが、
いくつかのパターンには対応できない可能性もある。
その場合は、ぜひ自分でジェネレーターを書いて、その要件を満たすコードを生成できるようにしてほしい。

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
    ...
```

## 開発用ツール

core/ast/tool(C++)や ast2go/gen(Go) ディレクトリ内にいくつか便利なツールが用意されている

tool/Stringer クラス(C++)|gen/ExprStringer クラス(Go) - Expr 型ノードをその言語の式の文字列に変換する
