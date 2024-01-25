---
title: "Source(Core)"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# Source(Core)

このページでは [開発ページの src/core ディレクトリ](https://github.com/on-keyday/brgen/tree/main/src/core)の中身の説明を行う

src/core ディレクトリには brgen(lang)の解析のコアとなる部分が入っている。

TODO(on-keyday): 説明を加える

```mermaid
flowchart LR
core-->ast
ast-->node
ast-->tool
ast-->builtin
core-->lexer
core-->middle
core-->common
```

AST 解析プログラム = tool/src2json は以上を以下のような関係で使っている

```mermaid
flowchart
ast/stream.h/Streamクラス-->|逐次呼び出し|lexer/lexer.h/parse_one関数
ast/parse.h/Parserクラス-->|トークナイザとして使用|ast/stream.h/Streamクラス
tool/src2json-->|パーサー呼び出し|ast/parse.h/Parserクラス
ast/parse.h/Parserクラス-->|AST|tool/src2json


tool/src2json-->|入力ファイルの管理|common/file.h/FileSetクラス

common/file.h/FileSetクラス-->|個別のファイルの管理|common/file.h/Fileクラス

common/file.h/Fileクラス-->|入力|ast/stream.h/Streamクラス

tool/src2json-->|ASTの意味解析/ASTノードの置換|middle/各関数
tool/src2json-->|importの解決|middle/resolve_import.h
tool/src2json-->|ASTの型解析|middle/typing.h/Typingクラス

```

## 開発者メモ

### StructType を make_shared している箇所

- Program
- IndentBlock
- ScopedStatement(match の=>の右側)
- BuiltinObject

### StructType を使っている箇所とそのときの base の型

- ファイル全体 base: Program
- match の =>/: のノード base: MatchBranch
- if/elif/else のノード base: If
- for のノード base: For
- format のノード base: Format
- state のノード base: State
- fn のノード base: Function
- builtin のノード base: BuiltinObject

### Scope の切り替わり箇所とそのときの owner の型

- ファイル全体 owner: Program
- match の条件節 owner: Match
- match の =>/: のノード owner: MatchBranch
- if/elif/else のノード owner: If
- if/elif の条件節 owner: If
- for のノード owner: For
- for の条件節 owner: For
- format のノード base: Format
- state のノード base: State
- fn のノード base: Function

### Ident.base の設定箇所とそのときの base の型

- union 型 field のノード base: Field
- field のノード base: Field
- assign のノード base: Binary
- enum のノード base :Enum
- enum メンバーのノード base: EnumMember
- format のノード base: Format
- state のノード base: State
- fn のノード base: Function
- builtin のノード base: BuiltinMember
- member_access のメンバー識別子 base: MemberAccess
- typing/typing_ident で行われる型付け base: Ident

### その他メモ

- 型名の識別子(Ident)の expr_type は nullptr

```brgen
format A: <--ここの識別子や
    ..

A # <--ここの識別子のexpr_typeはnullptr
```

- Typing クラス実装時 StructType を不用意に用いると木構造が再帰して木構造でなくなる場合があるので注意。代わりに IdentType を用いること。
- IdentType の base は IdentType にならないようにすること。

{{< mermaid >}}
