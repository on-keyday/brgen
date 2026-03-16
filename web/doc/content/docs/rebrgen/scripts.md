---
title: "Script Reference"
weight: 5
---

{{< hint info >}}
このドキュメントは AI (Claude) によって生成されたものです。内容に誤りがある場合は [GitHub Issue](https://github.com/on-keyday/brgen/issues) で報告してください。
{{< /hint >}}

# Script Reference

`rebrgen/script/` 以下のスクリプト一覧です。

## ビルド

| スクリプト | 説明 |
|----------|------|
| `build.py` | メインビルドスクリプト。cmake + ninja で C++ をビルド。`native` モードと `web` (Emscripten/WASM) モードをサポート |
| `build.ps1` / `build.bat` / `build.sh` | `build.py` のシェルラッパー |
| `auto_setup.py` | 初回セットアップ。`build_config.template.json` が存在しない場合に `build_config.json` を作成。サブモジュールの初期化と依存関係のビルドを行う |

## コード生成・テンプレート

| スクリプト | 説明 |
|----------|------|
| `ebmcodegen.py <lang>` | `tool/ebmcodegen` をラップして `src/ebmcg/ebm2<lang>/` のスケルトンを生成 |
| `ebmtemplate.py` | ビジターフックファイルの管理 (作成・コメント更新・テスト生成) |
| `ebmwebgen.py` | Web 用 JavaScript グルーコード (`ebm_caller.js` など) を自動生成 |
| `ebmdsl.py` | `src/ebmcg/*/dsl/` 以下の `.dsl` ファイルを C++ フック (`visitor/dsl/*.hpp`) にコンパイル |
| `update_ebm.py` | `extended_binary_module.bgn` 変更後の全生成ファイルの一括再生成 |

## テスト・実行

| スクリプト | 説明 |
|----------|------|
| `unictest.py` | 自動テストフレームワーク。EBM 生成・コードジェネレーター実行・テストスクリプト実行を統合 |
| `ebmtest.py` | `ebmgen` の EBM JSON 出力をスキーマ定義とテストケースに対して検証 |

## ビルド設定

`build_config.json` (テンプレート: `build_config.template.json`) の主な設定項目:

| キー | 説明 |
|-----|------|
| `AUTO_SETUP_BRGEN` | `true` にすると brgen ツールを自動ビルド |
| `AUTO_SETUP_FUTILS` | `true` にすると futils 依存関係を自動セットアップ |
| `CODEGEN_TARGET_LANGUAGE` | ビルドするコードジェネレーター言語の配列 |
| `INTERPRET_TARGET_LANGUAGE` | ビルドするインタープリタージェネレーター言語の配列 |

`CODEGEN_ONLY=1` 環境変数を設定すると `ebmcodegen` のみをビルド対象にします (EBM 構造の Bootstrap 修復時に使用)。

---

{{< hint warning >}}
以下のスクリプトは旧世代 (`bmgen`) 向けのもので、現在は使用しません。歴史的な参照として残しています。
{{< /hint >}}

## 旧世代スクリプト (bmgen)

`gen_template.py`, `generate.py`, `collect_cmake.py`, `run_generated.py`, `run_cmptest.py`, `generate_test_glue.py`, `generate_golden_master.py`, `test_compatibility.py`, `run_cycle.py`, `split_dot.py` など。
