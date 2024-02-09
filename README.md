# brgen - BinaRy encoder/decoder GENerator

ネットワーク・プロトコルのパケットやファイルフォーマットなどの解析/生成のためのコードを吐くジェネレーター

the generators that generate encoder/decoder code for parse/create network packet, binary file, etc...

# 目標(Goal)

- lightweight or no runtime - ランタイムは軽いもしくは無い
- enough to represent formats - 世の中にあるネットワークプロトコルフォーマットを表現するのに十分な表現力
- easy to write - 簡単に書ける
- write once generate any language code - 一回書けば様々な言語で生成

# Document

https://on-keyday.github.io/brgen/doc

# Playground

https://on-keyday.github.io/brgen

F1 キー(もしくは右クリック->Command Palette)で表示されるコマンドパレットに load example file と入力いただきますと、サンプルファイルがロードできます

You can load the sample file by typing load example file in the command palette displayed by pressing the F1 key (or right-click -> Command Palette).

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

All source code is released under the MIT license. The copyright remains with on-keyday.

# Issue Policy

機能の改善、提案、バグ報告などは [このリポジトリの GitHub Issue](https://github.com/on-keyday/brgen/issues/new) で受け付けています。
セキュリティ脆弱性等の報告方針は SECURITY.md を参照してください

We welcome Feature improvements, requests, and bug reports etc... on [this repository's GitHub Issue](https://github.com/on-keyday/brgen/issues/new).
see SECURITY.md about security vulnerability report policy.

# Pull Request Policy

Issue と同様に受け付けています。
ただし、マージする場合は作者(リポジトリオーナー)に意図がわかる説明(目的、変更点など)をつけることと CI にパスすることを求めます。

We also welcome PR as same as Issue. But, you have to explain about PR (purpose,change,etc.) for the author (repository owner) and pass the CI test if you request merge the PR.

# Version Policy

0.0.x の間は作者(リポジトリオーナー)都合で更新を行います。
0.1.0 以降については別途定める予定です。

Updates for versions 0.0.x will be made at the discretion of the author (repository owner). For versions 0.1.0 and beyond, separate guidelines will be established.

# Security Policy

SECURITY.md を参照してください

see SECURITY.md
