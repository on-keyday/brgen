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
以下が図になります。

```mermaid
flowchart TD;
brgen.json-.->|入力|brgen
brgen-->|呼び出し|src2json
input_file[定義ファイル]-.->|入力|src2json
src2json-.->|読み込み|input_file[定義ファイル]
src2json-->|AST|brgen
brgen-->|呼び出し/AST|generator
generator-->|生成結果|brgen
brgen-.->|生成結果書き出し|output_file[生成ファイル]

```

(点線は外部(ファイルシステム)とのやりとりを表します)

{{< mermaid >}}
