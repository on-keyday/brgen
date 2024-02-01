---
title: "Overview"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# Overview

brgen はバイナリパーサジェネレーター、およびその定義言語、ジェネレーター開発ツールの総称です。

以下 brgen(gen)をジェネレーター(正確にはジェネレータードライバー)、brgen(lang)を定義言語、brgen(tool)を開発ツールのこととして記載しています。

brgen(gen) はネットワークプロトコルやファイルフォーマットのバイナリエンコーダー/デコーダーを自動生成するためのツールです。
brgen(gen) は正確にはジェネレータードライバーといって、設定ファイル(brgen.json)にしたがって適切な brgen(lang)パーサーを呼び出し、その結果をジェネレーターに渡し、さらにその結果をファイルに書き込むというプログラムです。

brgen(CLI)

```mermaid
flowchart
brgen.json-.->|入力|brgen
brgen-->|呼び出し|src2json
input_file[定義ファイル]-.->|入力|src2json
src2json-.->|読み込み|input_file[定義ファイル]
src2json-->|AST|brgen
brgen-->|呼び出し/AST|generator
generator-->|生成結果|brgen
brgen-.->|生成結果書き出し|output_file[生成ファイル]

```

```mermaid
sequenceDiagram
FileSystem ->>+ brgen: brgen.json
brgen ->>+ src2json: 定義ファイル名+オプション(brgen.jsonに基づく)
FileSystem ->>+ src2json: 定義ファイル読み込み
src2json ->>+ brgen: 解析結果(AST)
brgen ->>+ generator: 起動/AST受け渡し(brgen.jsonに基づく)
generator ->>+ brgen: 生成結果(ソースコード)
brgen ->>+ FileSystem :生成結果書き込み

```

VSCode Extension

```mermaid
flowchart
input_file-.->|入力|VSCode
VSCode-.->|編集|input_file
VSCode-->|呼び出し|brgen-lsp[LSPサーバー]
brgen-lsp-->|呼び出し/入力|src2json
brgen-lsp-->|解析結果|VSCode
src2json-->|AST|brgen-lsp
```

```mermaid
sequenceDiagram
FileSystem ->>+ VSCode: 定義ファイル
VSCode ->>+ LSP-Server: 定義ファイル変更通知
LSP-Server ->>+ src2json: 定義ファイル送信(pipe)
src2json ->>+ LSP-Server:解析結果(AST)
LSP-Server ->>+ VSCode:形式変換/解析結果
VSCode ->>+ FileSystem: 変更保存
```

WebPlayground

```mermaid
flowchart
monaco-editor-->|変更検知|web-pg
web-pg[WebPlaygroundフロントエンド]-->|呼び出し/入力|src2json-->|AST|web-pg
web-pg-->|呼び出し/AST|generator
generator-->|生成結果|web-pg
web-pg-->|生成結果|monaco-editor
```

```mermaid
sequenceDiagram
Monaco-Editor ->>+ WebPlaygroundフロントエンド: 定義エディター変更通知
WebPlaygroundフロントエンド ->>+ WebWorker(src2json): 定義ファイル送信
WebWorker(src2json) ->>+ src2json(wasm): 定義ファイル送信(via cmdline arg)
src2json(wasm) ->>+ WebWorker(src2json): 解析結果(AST)
WebWorker(src2json) ->>+ WebPlaygroundフロントエンド : 解析結果(AST)
WebPlaygroundフロントエンド ->>+ WebWorker(generator) : 解析結果(AST)送信
WebWorker(generator) ->>+ generator(wasm) : 解析結果(AST)送信
generator(wasm) ->>+ WebWorker(generator) : 生成コード
WebWorker(generator) ->>+ WebPlaygroundフロントエンド : 生成コード
WebPlaygroundフロントエンド　->>+ Monaco-Editor : 生成コードエディター変更
```

プロダクト全体像

```mermaid
flowchart

コードジェネレーター-->C++ジェネレーター
コードジェネレーター-->Goジェネレーター
定義ファイルフォーマット-->定義ファイルパーサー
定義ファイルフォーマット-->定義サンプル
定義ファイルパーサー-->C++用AST
C++用AST-->C++ジェネレーター
C++用AST-->ASTジェネレーター
定義ファイルパーサー-->LSPサーバー&VSCodeExtension
定義ファイルパーサー-->WebPlayground
コードジェネレーター-->WebPlayground
ASTジェネレーター-->Go用AST
ASTジェネレーター-->Typescript用AST
Go用AST-->Goジェネレーター
Typescript用AST-->WebPlayground
Typescript用AST-->LSPサーバー&VSCodeExtension
定義ファイルパーサー-->CLI
コードジェネレーター-->CLI
```

{{< mermaid >}}
