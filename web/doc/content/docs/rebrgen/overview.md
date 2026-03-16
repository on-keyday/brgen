---
title: "Overview"
weight: 1
---

{{< hint info >}}
このドキュメントは AI (Claude) によって生成されたものです。内容に誤りがある場合は [GitHub Issue](https://github.com/on-keyday/brgen/issues) で報告してください。
{{< /hint >}}

# EBM / rebrgen Overview

## プロジェクトの目的

rebrgen は brgen のコードジェネレーター構築フレームワークで、`rebrgen/` ディレクトリに統合されています。

オリジナルの brgen が **AST-to-Code** モデルを使用するのに対し、rebrgen は **AST-to-IR-to-Code** モデルを採用し、中間表現 (IR) を介することで複数言語への展開を容易にしています。

```
.bgn ファイル
    → [src2json]     → brgen AST (JSON)
    → [ebmgen]       → EBM (Extended Binary Module)
    → [ebm2<lang>]   → ターゲット言語コード
```

## EBM (Extended Binary Module)

EBM は `src/ebm/extended_binary_module.bgn` で定義されるグラフ構造の IR です。

**設計の特徴:**

- **集中テーブル方式**: Statement・Expression・Type などのオブジェクトはそれぞれの集中テーブルに格納され、ID (Ref) で相互参照する
- **構造化制御フロー**: if / loop / match を保持し、コード生成時に各言語の構文へマッピングする
- **バイナリ意味論の保持**: エンディアン・ビットサイズなどのバイナリフォーマット固有の情報を IR レベルで保持する
- **段階的な低レベル化**: IO 操作をより低レベルなステートメントに変換するトランスフォームパスを持つ

`extended_binary_module.hpp/cpp` は `.bgn` ファイルから自動生成されるため、直接編集してはならない。

## コンポーネント構成

### ebmgen — AST → EBM 変換

`src/ebmgen/` が担当するパイプライン:

1. **convert**: brgen AST の各ノードを EBM に変換
2. **transform** (7パス): IO の平坦化 → CFG + ビット IO 低レベル化 → ビットフィールドのマージ → IO のベクタ化 → プロパティ導出 → キャスト追加 → 未使用コード削除
3. **interactive**: XPath 風クエリエンジンを使ったインタラクティブデバッガー

詳細は [ebmgen / ebmcodegen]({{< relref "ebmgen" >}}) を参照。

### ebmcodegen — メタジェネレーター

`src/ebmcodegen/` は言語ジェネレーターのスケルトンを生成するコードジェネレーターのジェネレーターです。

EBM 構造をビジターパターンで走査し、各言語ジェネレーター (`ebm2<lang>`) の C++ コード (主に `main.cpp` と `codegen.hpp`) を自動生成します。

生成されたコードは `src/ebmcg/ebm2<lang>/main.cpp` に書き出されるため、直接編集してはならない。

### 言語ジェネレーター

**コンパイル型** (`src/ebmcg/`): `ebm2c`, `ebm2go`, `ebm2python`, `ebm2rust`, `ebm2p4`, `ebm2zig`, `ebm2llvm`, `ebm2z3`

**インタープリター型** (`src/ebmip/`): `ebm2rmw` (RMW インタープリター), `ebm2json` (EBM JSON 出力)

詳細は [Current Status]({{< relref "status" >}}) を参照。

## フック階層

言語ジェネレーターはビジターフック (`*_class.hpp`) を介してコード生成ロジックを実装します。フックは以下の優先順位で解決されます:

1. **言語固有オーバーライド**: `src/ebmcg/ebm2<lang>/visitor/<Hook>_class.hpp`
2. **DSL 生成オーバーライド**: `visitor/dsl/<Hook>_dsl.hpp` (実験的)
3. **デフォルトフォールバック**: `src/ebmcodegen/default_codegen_visitor/visitor/<Hook>.hpp`

デフォルトフォールバックには多数のフック実装が含まれており、言語設定を調整することで挙動をカスタマイズできる。言語ジェネレーターは必要な部分だけオーバーライドすればよい。

詳細は [Visitor Hooks]({{< relref "hooks" >}}) を参照。

## 自動生成ファイル

以下のファイルはツールによって自動生成されるため、**直接編集しないこと**:

| ファイル | 生成元 |
|---------|-------|
| `src/ebm/extended_binary_module.cpp/hpp` | `extended_binary_module.bgn` から `json2cpp2` で生成 |
| `src/ebmcg/ebm2<lang>/main.cpp` | `ebmcodegen` が生成 |
| `src/ebmcodegen/body_subset.cpp` | `ebmcodegen --mode subset` が生成 |
| `src/ebmgen/json_conv.cpp/hpp` | `ebmcodegen` が生成 |

EBM 構造を変更した場合は `python script/update_ebm.py` で一括再生成する。
