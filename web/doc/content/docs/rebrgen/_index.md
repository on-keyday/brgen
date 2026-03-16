---
title: "EBM / rebrgen"
weight: 10
bookCollapseSection: true
---

{{< hint info >}}
このセクションのドキュメントは AI (Claude) によって生成・整理されたものです。内容に誤りがある場合は [GitHub Issue](https://github.com/on-keyday/brgen/issues) で報告してください。
{{< /hint >}}

# EBM / rebrgen

**rebrgen** は brgen の AST-to-IR-to-Code パイプラインを実装するコードジェネレーター構築フレームワークです。
元々は独立リポジトリでしたが、現在は brgen リポジトリに統合されています (`rebrgen/` ディレクトリ)。

## アーキテクチャ概要

```
.bgn ファイル
    → [src2json] → brgen AST (JSON)
    → [ebmgen]   → Extended Binary Module (EBM)
    → [ebm2<lang>] → ターゲット言語コード
```

EBM (Extended Binary Module) はグラフベースの中間表現 (IR) で、ステートメント・式・型などのオブジェクトをIDで参照する集中テーブル方式を採用しています。

## ドキュメント一覧

| ページ | 内容 |
|--------|------|
| [Overview]({{< relref "overview" >}}) | プロジェクト概要・EBM の設計思想 |
| [ebmgen / ebmcodegen]({{< relref "ebmgen" >}}) | ツールの使い方・CLI リファレンス |
| [Visitor Hooks]({{< relref "hooks" >}}) | コード生成ロジックの実装方法 |
| [Testing]({{< relref "testing" >}}) | unictest による自動テスト |
| [Script Reference]({{< relref "scripts" >}}) | ビルド・生成スクリプト一覧 |
| [EBM API Reference]({{< relref "ebm_api" >}}) | EBM Magic Access Path リファレンス |
| [Current Status]({{< relref "status" >}}) | 開発状況・各言語ジェネレーターの成熟度 |
