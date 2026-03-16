---
title: "ebmgen / ebmcodegen"
weight: 2
---

{{< hint info >}}
このドキュメントは AI (Claude) によって生成されたものです。内容に誤りがある場合は [GitHub Issue](https://github.com/on-keyday/brgen/issues) で報告してください。
{{< /hint >}}

# ebmgen / ebmcodegen

## ebmgen

`tool/ebmgen` は brgen AST (JSON) または `.bgn` ファイルを EBM バイナリに変換するツールです。

### 基本的な変換

```bash
# JSON AST → EBM
./tool/ebmgen -i input.json -o output.ebm

# .bgn ファイルを直接変換 (libs2j が必要)
./tool/ebmgen -i input.bgn -o output.ebm

# EBM をデバッグテキストに出力
./tool/ebmgen -i input.ebm -d debug.txt

# EBM を JSON 形式でデバッグ出力
./tool/ebmgen -i input.ebm -d debug.json --debug-format json
```

### コマンドラインオプション

| フラグ | 短縮 | 説明 |
|--------|------|------|
| `--input` | `-i` | **(必須)** 入力ファイルを指定 |
| `--output` | `-o` | 出力 EBM バイナリファイル。`-` で標準出力 |
| `--debug-print` | `-d` | デバッグ出力先ファイル。`-` で標準出力 |
| `--input-format` | | 入力フォーマットを明示指定: `bgn`, `json-ast`, `ebm` (デフォルト: 拡張子で自動判定) |
| `--debug-format` | | デバッグ出力フォーマット: `text` (デフォルト), `json` |
| `--interactive` | `-I` | インタラクティブデバッガーを起動 |
| `--query` | `-q` | コマンドラインからクエリを実行 |
| `--query-format` | | クエリ出力フォーマット: `id` (デフォルト), `text`, `json` |
| `--cfg-output` | `-c` | 制御フローグラフ (CFG) を指定ファイルに出力 |
| `--libs2j-path` | | `.bgn` ファイル変換用 `libs2j` 動的ライブラリのパス |
| `--debug` | `-g` | デバッグモード (未使用アイテムを削除しない等) |
| `--verbose` | `-v` | 詳細ログを有効化 |
| `--timing` | | 各ステップの処理時間を出力 |
| `--base64` | | Web Playground 互換の base64 エンコードで出力 |
| `--show-flags` | | コマンドラインフラグの説明を JSON 形式で出力 |

### インタラクティブクエリエンジン

EBM の内部構造をクエリで検査できます。

```bash
# インタラクティブモード
./tool/ebmgen -i input.ebm --interactive

# コマンドラインからクエリ実行
./tool/ebmgen -i input.ebm -q "Statement { body.kind == \"IF_STATEMENT\" }"
```

**クエリ構文**: `<ObjectType> { <conditions> }`

- オブジェクト型: `Identifier`, `String`, `Type`, `Statement`, `Expression`, `Any`
- 条件演算子: `==`, `!=`, `>`, `>=`, `<`, `<=`, `and`, `or`, `not`, `contains`
- フィールドアクセス: `.` でメンバーアクセス、`->` で Ref 型を逆参照、`[]` でインデックスアクセス

詳細は [EBM API Reference]({{< relref "ebm_api" >}}) を参照。

---

## ebmcodegen

`tool/ebmcodegen` は言語ジェネレーターのスケルトンを生成するメタジェネレーターです。通常は `script/ebmcodegen.py` 経由で呼び出します。

### 新しい言語ジェネレーターの追加

```bash
# 1. スケルトン生成 (src/ebmcg/ebm2<lang>/ を作成)
python script/ebmcodegen.py <lang_name>

# 2. ビルド
python script/build.py

# 3. テスト実行 (未実装フックを確認)
python script/unictest.py --target-runner ebm2<lang_name>
```

### 主なモード (`--mode`)

| モード | 説明 |
|--------|------|
| `codegen` | (デフォルト) コードジェネレータースケルトンを生成 |
| `interpret` | インタープリタースケルトンを生成 |
| `hooklist` | 利用可能なフック一覧を表示 |
| `hookkind` | フックの種別情報を表示 |
| `template` | 単一フックのテンプレートを出力 |
| `subset` | `body_subset.cpp` を生成 |
| `dsl` | DSL ファイルを C++ フックに変換 |
| `spec-json` | EBM スキーマを JSON 形式で出力 |
| `json-conv-header` | `json_conv.hpp` (JSON デシリアライズヘッダー) を生成 |
| `json-conv-source` | `json_conv.cpp` (JSON デシリアライズ実装) を生成 |
| `accessor` | アクセサーコードを生成 |
| `ebmgen-visitor` | ebmgen ビジターコードを生成 |
| `codegen-class-header` | クラスベースコードジェネレーター用ヘッダーを生成 |
| `codegen-class-source` | クラスベースコードジェネレーター用ソースを生成 |
| `interpret-class-header` | クラスベースインタープリター用ヘッダーを生成 |
| `interpret-class-source` | クラスベースインタープリター用ソースを生成 |

---

## EBM 構造の更新

`extended_binary_module.bgn` を変更した場合:

```bash
python script/update_ebm.py
```

このスクリプトは以下を順番に実行します:

1. `src/ebm/ebm.py` で `extended_binary_module.hpp/cpp` を再生成
2. `script/build.py` でビルド
3. `ebmcodegen --mode subset` で `body_subset.cpp` を生成
4. `ebmcodegen --mode json-conv-header` で `json_conv.hpp` を生成、`--mode json-conv-source` で `json_conv.cpp` を生成
5. 変更があった場合は再ビルド
6. `extended_binary_module.bgn` を hex テストデータに変換

**注意**: `ebmcodegen` 自体が更新対象のファイルに依存しているため、EBM 構造の変更が `ebmcodegen` をビルド不能にした場合は:

```bash
export CODEGEN_ONLY=1
python script/build.py      # ebmcodegen のみビルド
export CODEGEN_ONLY=0
python script/update_ebm.py  # 通常の更新フロー
```
