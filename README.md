# brgen - BinaRy encoder/decoder GENerator

ネットワーク・プロトコルのパケットやファイルフォーマットなどの解析/生成のためのコードを吐くジェネレーター
及びバイナリフォーマット定義言語

the generators that generate encoder/decoder code for parse/create network packet, binary file, etc...　 and binary format definition language

# 目標(Goal)

- enough to represent formats - 世の中にあるネットワークプロトコルフォーマットを表現するのに十分な表現力
- easy to write and read - 簡単に書ける/読める
- write once generate any language code - 一回書けば様々な言語で生成

# Document

https://on-keyday.github.io/brgen/doc

# Playground

https://on-keyday.github.io/brgen

F1 キー(もしくは右クリック->Command Palette)で表示されるコマンドパレットに load example file と入力いただきますと、サンプルファイルがロードできます

You can load the sample file by typing load example file in the command palette displayed by pressing the F1 key (or right-click -> Command Palette).

# Design Doc

## このプロダクトの目標

本プロダクトの最終的な目標(大それた野望とも言う)はバイナリフォーマットの仕様書に brgen の定義言語をつけることが常識となることである。
それによってバイナリフォーマットの仕様書を手に入れたら即コードジェネレーターに生成させることができ、
そして実際に使用をすることも簡単になるであろう。また、一意に定まる構文によって文章や図表による曖昧さがなくなり、
より初学者やプログラマーにとって理解しやすいものになるであろう。

もちろん現実的にはバイナリフォーマットを開発した人がそれを扱うためのライブラリを提供しているだったり、速度的な制約等がありこれらのコードジェネレーターでは無理という場合、
他にも何かしら都合が悪かったりして使えない場合もあるだろう。
しかし、意味的に曖昧さのない定義書が存在することはそういった手書きで実装する際にも大いに役に立つであろう。

## 設計概要

本プロダクトはコアの部分としてバイナリフォーマット定義言語(brgen(lang)と表記する)のパーサーと
コードジェネレーターの２つの部分に分けられる。
brgen(lang)のパーサーは本プロダクトでは C++で書かれており、これが同時に定義言語の文法を決定しているという面がある。
そしてコードジェネレーターは C++向けを C++で、 Go 向けは Go で、Rust 向けは Rust で書かれつつある。これは、brgen(lang)のパース結果が様々な言語で扱えることを示す PoC 的な意味合いが強く、実際開発する際は、
かならずしもその言語自身での開発が要求されるわけではない。

## 開発方針

本プロダクトは、まず最小限でいいから動くものを作り、そこから徐々に成長させていくという
方針で作っている。実際本リポジトリには多くの書きかけの、しかし一応は動くというコードがかなりの数ある。
また、brgen のコードジェネレーター自体の実装方針も全部が全部すべての言語仕様を出力できなくても良いという方針を敷いている。
将来的にはこれらの完成度合いを表す指標を導入したいと考えている。
現段階で CI/CD をしてはいるが、現状ビルド可能かと tool\brgen と入力した際に、[brgen.json](https://github.com/on-keyday/brgen/blob/main/brgen.json)に書いたジェネレーターが動く(動くというのは実行可能であるかであり、仕様通りに動くかではない)ことしか検証していないため、今後より強化していきたい。

# Design Document

## Goals of the Product

The ultimate goal of this product (some might call it a lofty ambition) is to make it common practice to include the definition language of `brgen` in binary format specifications. This would enable individuals to obtain a binary format specification and immediately generate code using a code generator. Consequently, using the format would become much simpler. Additionally, having a syntax that is unambiguous would eliminate ambiguities found in textual descriptions and diagrams, making it more accessible to beginners and programmers alike.

Of course, in reality, there may be constraints such as the developers of the binary format providing libraries for handling it, or limitations in terms of speed that make it impossible to use such code generators. However, having a definition document that is unambiguous in meaning would still be immensely helpful even in scenarios where manual implementation is necessary.

## Design Overview

This product can be divided into two core components: the parser for the binary format definition language (`brgen(lang)`) and the code generator.

- The parser for `brgen(lang)` is implemented in C++. It simultaneously determines the grammar of the definition language.
- The code generator is being developed separately for C++, Go, and Rust. This serves as a proof of concept showing that the parsing results of `brgen(lang)` can be handled in various languages. However, it is not mandatory to develop in the same language as the target language when actually developing.

## Development Approach

The development approach for this product is to create a minimum viable product first and then gradually expand its capabilities. The repository contains many incomplete but somewhat functional pieces of code. Additionally, the implementation strategy for the `brgen` code generator itself does not require it to be able to output all language specifications perfectly. In the future, we aim to introduce metrics to indicate the completeness of these implementations.
At the current stage, while we are implementing CI/CD, we are only verifying whether the project is buildable and whether the generators listed in brgen.json actually run (by "run", I mean they are executable, not necessarily that they function as specified) when the command tool\brgen is input. We haven't yet validated whether they function according to specifications. Therefore, we aim to enhance our CI/CD process in the future.

# 謝辞

本作品は [SecHack365'23](https://sechack365.nict.go.jp/) の作品として作り始められました。
成果発表会時点のコードは SecHack365-final タグのコミットになります。

# examples

https://github.com/on-keyday/brgen/tree/main/example を御覧ください

TODO(on-keyday): 現在、example 内のコードのいくつかは実装されていない機能が使われており、ジェネレーターで生成できる保証がありません

see https://github.com/on-keyday/brgen/tree/main/example

TODO(on-keyday): some of these examples use non-implemented functionality and not working with current generator implementation

# How to Use (simple)

1.  `brgen.json`の`input_dir`を入力ファイルのあるディレクトリ`output_dir`の項目を出力先ディレクトリに設定(デフォルトでは入力は`example`ディレクトリ、出力は`ignore/example/<language name>`となっています)
1.  `tool/brgen`を実行
1.  出力先ディレクトリにコードが生成されます

現在、生成されたコードが言語の構文通りかはチェックされません。別途コンパイラ等で確認してください。

# How to build

## 必要なもの

- cmake (必須)
- clang++ (必須)
- ninja (必須)
- go (必須)
- emscripten/emsdk (wasm/build_all のときのみ)
- npm (wasm/build_all のときのみ)
- webpack (wasm/build_all のときのみ)
- typescript (wasm/build_all のときのみ)
- python (build_all のときのみ)
- vsce (build_all のときのみ)

## ネイティブ

- linux 環境の場合:
  1.  `. build.sh`を実行
- windows 環境の場合:
  1.  build.bat を開き、FUTILS_DIR 環境変数を`.\utils`に設定する
  2.  `build.bat`を実行

## wasm(web)

- linux 環境の場合
  1. `emsdk_env`を呼び出す
  1. `. build.sh wasm-em`を実行
- windows 環境の場合
  1.  `emsdk_env.bat`を呼び出す
  1.  build.bat を開き、FUTILS_DIR 環境変数を`.\utils`に設定する
  1.  `build.bat wasm-em`を実行

## build_all

- windows 環境のみ
  1. `EMSDK_PATH`を`emsdk_env.bat`のパスに設定する
  1. build.bat を開き、FUTILS_DIR 環境変数を`.\utils`に設定する
  1. `build_all.bat`を実行

# License

MIT License

All source code is released under the MIT license.
The copyright remains with on-keyday and contributors.
if you want to contribute this product, you must agree with publishing your code under the MIT license.

You can decide license of code that is generated from code generator (and not contained in this repository) freely.

For built binary released at GitHub Release,
license of dependency are collected by [licensed](https://github.com/github/licensed) and [gocredits](https://github.com/Songmu/gocredits) and bundled with released binaries.

If you find license problem, please tell us via GitHub Issue.
see also [license_note.txt](https://github.com/on-keyday/brgen/blob/main/script/license_note.txt)

# Issue Policy

機能の改善、提案、バグ報告などは [このリポジトリの GitHub Issue](https://github.com/on-keyday/brgen/issues/new) で受け付けています。
セキュリティ脆弱性等の報告方針は SECURITY.md を参照してください

We welcome Feature improvements, requests, and bug reports etc... on [this repository's GitHub Issue](https://github.com/on-keyday/brgen/issues/new).
see SECURITY.md about security vulnerability report policy.

# Pull Request Policy

Issue と同様に受け付けています。
ただし、マージする場合は作者(リポジトリオーナー)に意図がわかる説明(目的、変更点など)をつけることと CI にパスすることを求めます。
以下のブランチ命名規則に従っていただけると作者に意図が伝わりやすくなると思います。

- doc/ - ドキュメントへの提案/変更
- lang/ - 言語仕様/言語パーサーへの提案/変更
- gen/ - brgen(lang)を入力とするコードジェネレーターへの提案/変更
- ast/ - AST コードジェネレーターや AST ハンドリングツールへの提案/変更
- web/ - WebPlayground への提案/変更
- lsp/ - LSP サーバーへの提案/変更
- env/ - 開発環境(shell script, GitHub Actions Workflow ファイル,Dockerfile など)への提案/変更
- tool/ - その他のツール(他言語から brgen 形式にするツールなど)の提案/変更
- sample/ - フォーマットのサンプルへの提案変更
- other/ - 以上以外で何かしらの

分類を増やすべきという場合やその他の事項は GitHub Issue で提案してください。

We welcome PR as well as issues. However, if you ask for a PR to be merged, you will need to provide the author (repository owner) with a description of your PR (purpose, changes, etc.) and pass a CI test.
Following the branch naming conventions below would make it easier for the author to understand the intention:

- doc/ - Proposals/changes to documentation
- lang/ - Proposals/changes to language specifications/language parsers
- gen/ - Proposals/changes to code generators taking brgen(lang) as input
- ast/ - Proposals/changes to AST code generators or AST handling tools
- web/ - Proposals/changes to WebPlayground
- lsp/ - Proposals/changes to LSP servers
- env/ - Proposals/changes to development environment (shell scripts, GitHub Actions Workflow files,Dockerfile, etc.)
- tool/ - Proposals/changes to other tools (tools to convert from other languages to brgen format, etc.)
- sample/ - Proposals/changes to format samples
- other/ - Anything else not covered above

If there is a need to add more categories or any other issues, please propose them through GitHub Issue.

# Version Policy

0.0.x の間は作者(リポジトリオーナー)都合で更新を行います。
0.1.0 以降については別途定める予定です。

Updates for versions 0.0.x will be made at the discretion of the author (repository owner). For versions 0.1.0 and beyond, separate guidelines will be established.

# Security Policy

SECURITY.md を参照してください

see SECURITY.md

# You can write generator

もし、お好みの言語がないのであれば自分でジェネレーターを書いていただくことも可能です。
現在、AST 操作用のライブラリを C++、Go、TypeScript、Rust、Python で提供しております。TODO(on-keyday): 各種パッケージ管理システムに公開する、操作ライブラリのドキュメントを書く

- C++: src/core/ast/ast.h と src/core/ast/traverse.h をから利用できます。その他、src/core/ast/tool ディレクトリに各種ツールがあります。
- Go: ast2go にあります。
- TypeScript: ast2ts にあります。
- Rust: ast2rust にあります。
- Python: ast2py にあります。

また、C や C#, Dart 向けはディレクトリはありますが、まだ書きかけなので正常に動作しません

もし、ジェネレーターを作成できて、提供して頂ける場合は src/tool ディレクトリに追加して Pull Request してください。

また、AST 操作用ライブラリ自体を提供していただくことも可能です。
詳しくは src/gen ディレクトリのソースコードを参照してください。 TODO(on-keyday): 作り方を書く。
もし、AST 操作用ライブラリを作成できて、提供して頂ける場合は src/tool/gen ディレクトリに追加して Pull Request してください。

If you don't have a language you like, you can write your own generator.
We currently provide libraries for AST manipulation in C++, Go, TypeScript, Rust, and Python. TODO(on-keyday): Publish to various package management systems, write document for AST manipulation libraries

- C++: src/core/ast/ast.h and src/core/ast/traverse.h are available. Other tools are located in the src/core/ast/tool directory.
- Go: Found in ast2go.
- TypeScript: Found in ast2ts.
- Rust: Found in ast2rust.
- Python: Found in ast2py.

Also, there are directories for C and C#, but they are still being written so they will not work properly.

If you can create a generator and provide it, please add it to the src/tool directory and submit a Pull Request.

It is also possible to provide the AST operation library itself.
See the source code in the src/gen directory for details. TODO(on-keyday): Write how to make it.
If you are able to create and provide a library for AST operation, please add it to the src/tool/gen directory and submit a Pull Request.
