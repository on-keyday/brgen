# `script/` ディレクトリのコマンド解説

このドキュメントは `script/` ディレクトリに含まれる各スクリプトの機能について解説します。

## ビルド関連

### `build.py` / `build.ps1` / `build.bat` / `build.sh`

プロジェクトのビルドを実行するスクリプトです。各ファイルは異なる実行環境に対応しています。

- **`build.py`**: メインとなる Python スクリプト。`cmake` と `ninja` を使用して、C++ソースコードのビルドプロセスを管理します。`native`（デスクトップ向け）と `web`（WebAssembly 向け）のビルドモードをサポートしています。
- **`build.ps1`**: PowerShell 用のラッパースクリプト。内部で `build.py` を呼び出します。
- **`build.bat`**: Windows のコマンドプロンプト用のラッパースクリプト。内部で `build.py` を呼び出します。
- **`build.sh`**: Linux/macOS などの Unix 系シェル用のラッパースクリプト。内部で `build.py` を呼び出します。

---

## コード生成・テンプレート関連

### `ebmcodegen.py`

`ebmgen` の IR である EBM (ExtendedBinaryModule) に基づいて、新しいコードジェネレータやインタプリタのスケルトンコードを生成します。`tool/ebmcodegen` の実行ファイルをラップし、指定された言語の `main.cpp` と `CMakeLists.txt` を `src/ebmcg/ebm2<lang>` や `src/ebmip/ebm2<lang>` に作成します。

### `ebmtemplate.py`

`ebmcodegen` を利用して、コードジェネレータの各 EBM ノードに対応する「ビジターフック」のテンプレートファイルを管理（作成・更新）するためのヘルパースクリプトです。

- **`ebmtemplate.py`**: 使い方の help がでます
- **`ebmtemplate.py <template_target>`**:　フックファイルの概要を標準出力に出力します
- **`ebmtemplate.py <template_target> <lang>`**: 新しいフックファイル (`.hpp`) を `src/ebmcg/ebm2<lang>/visitor/` に生成します。
- **`ebmtemplate.py update <lang>`**: 既存のフックファイルのコメントブロック（利用可能な変数一覧）を最新の状態に更新します。
- **`ebmtemplate.py test`**: 利用可能な全てのテンプレートターゲットの生成をテストします。

### `ebmwebgen.py`

`ebm`ベースの各コードジェネレータを Web 上で動作させるための JavaScript グルーコード（Web Worker や UI 連携のコード）を自動生成します。`web/tool` ディレクトリに `ebm_caller.js` などを出力します。

### `src/ebm/ebm.ps1`

(このスクリプトのみ`src/ebm`ディレクトリにある)
EBM (Extended Binary Module) の構造自体を定義した `extended_binary_module.bgn` ファイルから、対応する C++のヘッダファイル (`extended_binary_module.hpp`) とソースファイル (`extended_binary_module.cpp`) を生成する PowerShell スクリプトです。EBM の定義を変更した際に実行する必要があります。

### `gen_template.py` / `gen_template.ps1` / `gen_template.bat` / `gen_template.sh`

**[旧世代]`bmgen`** のためのスクリプト。指定された言語のコードジェネレータのテンプレート（`.hpp`, `.cpp`, `main.cpp`, `CMakeLists.txt`）を `src/old/bm2<lang>/` ディレクトリに生成します。`tool/gen_template` 実行ファイルをラップしています。

### `generate.py` / `generate.ps1` / `generate.bat` / `generate.sh`

**[旧世代]`bmgen`** のためのスクリプト。複数のスクリプトをまとめて実行し、プロジェクト全体のコード生成と設定更新を行います。主に以下の処理を実行します。

1.  `gen_template.py` を使用して、定義済みの各言語（C, Python, Go など）のジェネレータを生成。
2.  `collect_cmake.py` を実行して、トップレベルの `CMakeLists.txt` を更新。
3.  `generate_test_glue.py` を実行して、テスト用の設定ファイルを生成。
4.  `tool/gen_template` を使用して、ドキュメント (`docs/template_parameters.md`) を生成。

### `collect_cmake.py`

**[旧世代]`bmgen`** のためのスクリプト。`src/` ディレクトリ以下を探索し、各コードジェネレータの `CMakeLists.txt` を `add_subdirectory` する形でトップレベルの `src/CMakeLists.txt` を自動生成します。また、`run_generated.py` スクリプトも動的に生成します。

### `generate_web_glue.py`

**[旧世代]`bmgen`** のためのスクリプト。各コードジェネレータを Web アプリケーションとして動作させるための JavaScript グルーコードを生成します。`web/tool` ディレクトリに `bm_caller.js` などを出力します。

---

## テスト・実行関連

### `ebmtest.py`

`ebmgen` が出力した EBM の JSON (`.ebm`) が、スキーマ定義に準拠しているか、またテストケースで定義された期待値と一致するかを検証します。

- スキーマ仕様 (`ebmcodegen --mode spec-json` で取得) に基づいて JSON を検証します。
- テストケースファイルと比較し、Ref の解決を含めた深い等価性チェックを行います。

### `script/bm/run_cycle.py` / `script/bm/run_cycle.ps1` / `script/bm/run_cycle.bat` / `script/bm/run_cycle.sh`

開発サイクル全体を一度に実行するためのスクリプトです。以下のスクリプトを順番に呼び出します。

1.  `generate.py`: 全てのコードジェネレータと関連ファイルを生成。
2.  `run_generated.py`: 生成されたジェネレータを実行して、サンプルコードを生成。
3.  `run_cmptest.py`: 生成されたコードの互換性テストを実行。

### `run_generated.py` / `run_generated.ps1` / `run_generated.bat` / `run_generated.sh`

**[旧世代]`bmgen`** のためのスクリプト。`collect_cmake.py` によって自動生成されるスクリプトです。以下の処理を自動で行います。

1.  プロジェクトをビルド (`build.py`)。
2.  `.bgn` ファイルを `src2json` で JSON AST に変換。
3.  `bmgen` を実行してバイナリ (`.bin`) と DOT グラフ (`.dot`) を生成。
4.  `src/old/bm2*` にある各言語のジェネレータを実行し、`save/<lang>/` 以下にソースコードを生成。

### `run_cmptest.py` / `run_cmptest.ps1` / `run_cmptest.bat` / `run_cmptest.sh`

`brgen` リポジトリにある `cmptest` ツールを実行するためのラッパースクリプトです。`testkit/` ディレクトリに定義された設定に基づき、生成されたコードの互換性テストを行います。

### `generate_test_glue.py`

**[旧世代]`bmgen`** のためのスクリプト。`testkit/` ディレクトリに、`cmptest` で使用されるテスト設定ファイル (`cmptest.json`, `test_info.json` など) を自動生成します。各ジェネレータの `config.json` を読み取り、テストのセットアップを行います。

### `generate_golden_master.py`

**[旧世代]`bmgen`** のためのスクリプト。`gen_template` を使って全言語のジェネレータソースコードを生成し、`test/golden_masters/` ディレクトリに保存します。これは、`gen_template` の出力に意図しない変更がないかを確認するための「ゴールデンマスター」として機能します。

### `test_compatibility.py`

**[旧世代]`bmgen`** のためのスクリプト。`gen_template` の現在の出力と、`generate_golden_master.py` で作成されたゴールデンマスターとを比較します。これにより、テンプレート生成ロジックの変更が既存の出力に影響を与えていないか（後方互換性があるか）をテストします。

---

## ユーティリティ

### `binary_module.bat` / `binary_module.ps1`

**[旧世代]`bmgen`** の核となる `binary_module.bgn` から、C++のヘッダとソースファイル (`binary_module.hpp`, `binary_module.cpp`) を生成します。`brgen` の `src2json` と `json2cpp2` を使用します。

### `list_lang.py`

**[旧世代]`bmgen`** のためのスクリプト。`src/` ディレクトリ内を探索し、存在する `bm2<lang>` ディレクトリをリストアップして、対応している言語の一覧を JSON 形式で出力します。

### `split_dot.py`

**[旧世代]`bmgen`** のためのスクリプト。`ebmgen` が出力した大規模な DOT グラフファイルを、サブグラフごとに個別の DOT ファイルと SVG ファイルに分割します。これにより、複雑なグラフの可視性が向上します。
