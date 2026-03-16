---
title: "Current Status"
weight: 7
---

{{< hint info >}}
このドキュメントは AI (Claude) によって `rebrgen/docs/en/current_status.md` (2026-02 時点のスナップショット) を元に生成されたものです。実際の状態と乖離している可能性があります。最新状況は git log やソースコードで確認してください。
{{< /hint >}}

# Current Status

## 開発フェーズ

コアパイプライン (AST → EBM IR → ターゲットコード) は稼働中です。命名・冗長コードの整理は後回しにして機能を優先しており、言語ジェネレーターのカバレッジ拡大とデフォルトビジターへの処理集約が現在の主な作業です。

## 言語ジェネレーターの構成

### フック階層の仕組み

言語固有のフックファイル数は実装の完全性を**直接示すものではありません**。フレームワークには3段階のフォールバック (`__has_include` ベース) があります:

1. 言語固有オーバーライド (`visitor/<Hook>_class.hpp`)
2. DSL 生成オーバーライド (`visitor/dsl/<Hook>_dsl.hpp`)
3. デフォルトフォールバック (`ebmcodegen/default_codegen_visitor/visitor/<Hook>.hpp`)

デフォルトフォールバックが多くのフックを実装しているため、後から追加された言語ジェネレーターほど固有フックが少なく、デフォルトへの依存度が高い傾向があります。固有フックが少ない = 未完成ではありません。

### コンパイル型ジェネレーター (`src/ebmcg/`)

| ジェネレーター | 備考 |
|--------------|------|
| `ebm2rust` | 最初期。型システムのオーバーライドが多い |
| `ebm2c` | C のマクロベース I/O システムとヘッダーオンリー生成のため独自ロジックが多い |
| `ebm2python` | DSL サンプルファイルあり (`dsl_sample/`) |
| `ebm2p4` | P4 ネットワーク言語 |
| `ebm2go` | 最近追加。デフォルトレイヤーを最大限活用 |
| `ebm2zig` | Zig |
| `ebm2llvm` | LLVM IR |
| `ebm2z3` | Z3 SMT ソルバー |

### インタープリター型ジェネレーター (`src/ebmip/`)

| ジェネレーター | 備考 |
|--------------|------|
| `ebm2rmw` | `default_interpret_visitor` ベース。テキスト出力ではなく `Instruction` オブジェクトを生成する根本的に異なるアプローチ |
| `ebm2json` | EBM の JSON 出力 |

## コアフレームワークの状況

### EBM IR (`src/ebm/`)

- グラフ構造の IR。5つの集中テーブル (Identifier, String, Type, Statement, Expression)
- 参照は QUIC スタイルの Varint でエンコード。エイリアステーブル (`aliases`/`RefAlias`) で識別子・型・式の別名を管理
- 段階的な IO 低レベル化をサポート
- union 型・複合フィールド・デバッグソース位置のプロパティシステム

### ebmgen — AST → EBM 変換 (`src/ebmgen/`)

- 動作中のパイプライン: convert → add_files → トランスフォームパス (詳細は [Overview]({{< relref "overview" >}}) 参照) → finalize
- インタラクティブデバッガーと XPath 風クエリエンジンが稼働中
- 既知の未実装部分:
  - `expression.cpp`: 複雑な型変換 (ARRAY, VECTOR, STRUCT, RECURSIVE_STRUCT)
  - `expression.cpp`: 共通型推論はベストエフォート
  - `encode.cpp` / `decode.cpp`: 一部の引数処理が未完成
  - `statement.cpp`: 厳密な状態変数解析が未実装
  - `bit_holder.cpp`: struct 型マッピングが未完成

### ebmcodegen — メタジェネレーター (`src/ebmcodegen/`)

- 旧来の `#if __has_include` システムと新しいクラスベースシステムが共存
- クラスベースシステム: 7段階フック優先順位、before/after with hijack、IDE オートコンプリート
- `class_based.cpp` が最大の単一ソースファイル (~2000行)
- 既知の問題: CRTP を含むケースで `HasVisitor` concept が一部コンパイラーで正しく動作しない (マクロ回避策あり)

## ビルドと CI

- CMake 3.25+ / Ninja / Clang C++23
- ネイティブと Web (Emscripten/WASM) の2モード
- 自動生成の `main.cpp` は各言語で ~4MB、`codegen.hpp` は ~400KB (ビルド時間に影響)
- CI は Ubuntu のみ。Windows は開発環境だが CI 未対応

## 既知の技術的負債

1. **大きな自動生成ファイル**: `main.cpp` (~4MB) と `codegen.hpp` (~400KB) がビルド時間と IDE パフォーマンスに影響
2. **コンパイラー固定**: `clang++` のみサポート。GCC/MSVC 非対応
3. **Bootstrap の循環性**: `extended_binary_module.bgn` 変更時は慎重な多段階リビルドが必要 (`CODEGEN_ONLY=1` 参照)
4. **Windows CI なし**: 開発は Windows で行われるが CI テストは Ubuntu のみ
