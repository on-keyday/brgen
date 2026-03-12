# 用語集：上級者向け（EBM コア開発者）

このドキュメントは、`rebrgen`のコアツール（特に`ebmgen`）や EBM 自体の設計・開発に関わる開発者向けの用語集です。

## `ebmgen` (AST-to-EBM コンバータ)

- **ebmgen**

  - **役割**: `brgen`の AST（`.bgn`ファイルから生成）を EBM に変換するツール。

- **ConverterContext**

  - **役割**: AST から EBM への変換プロセス全体を統括する中心的なオブジェクト。
  - **構成**: `ConverterState`（変換中の一時状態）、`EBMRepository`（EBM の構築担当）などを保持し、各種コンバータへのアクセスを提供します。

- **EBMRepository**

  - **役割**: EBM を構築するコンポーネント。EBM の全オブジェクト（`Identifier`, `Type`, `Statement`など）の生成と ID 割り当てを管理します。
  - **特徴**: `ReferenceRepository`を内部に持ち、オブジェクトの重複を避けるためのキャッシュ機構を備えています。

- **ReferenceRepository**

  - **役割**: 特定の EBM オブジェクト（例: `Statement`）の集合を管理するテンプレートクラス。
  - **機能**: 新規オブジェクトの追加、ID 割りあて、重複回避のためのキャッシュロジックを担当します。

- **ConverterState**

  - **役割**: 変換処理中における一時的な状態を保持する構造体。
  - **内容**: 現在のエンディアン、処理中の AST ノード、循環参照や共有構造を正しく扱うための訪問済みノードキャッシュなどが含まれます。

- **MappingTable**
  - **役割**: EBM の構築後に、その内容への高速なアクセスを提供するためのクラス。
  - **機能**: オブジェクト ID をキーとするハッシュマップを構築し、コードジェネレータやデバッガが EBM の要素を効率的に検索できるようにします。

## EBM の自己生成と更新

- **src2json**

  - **役割**: `brgen`リポジトリ由来のツール。`.bgn`ファイルをパースし、`brgen`の AST を JSON 形式で出力します。`ebmgen`はこの機能を内部的に利用します。

- **json2cpp2**

  - **役割**: `brgen`リポジトリ由来のツール。JSON 定義から C++のコードを生成します。
  - **用途**: `rebrgen`では、EBM の構造定義ファイルである`extended_binary_module.bgn`から、EBM 自体の C++クラス定義（`.hpp`/`.cpp`）を生成するために使われます。

- **ebm.py / ebm.ps1**

  - **役割**: `extended_binary_module.bgn` を基に、`json2cpp2` を呼び出して EBM の C++クラス定義 (`.hpp`/`.cpp`) を直接生成するスクリプト。
  - **位置づけ**: `update_ebm.py` フローの一部であり、EBM 構造の更新における最初のステップです。

- **update_ebm.py**
  - **役割**: EBM のコア構造（`extended_binary_module.bgn`）が変更された際に、関連する全ての生成ファイルを更新するプロセスを自動化するスクリプト。
  - **動作**: C++定義の再生成、ツールのリビルド、メタデータファイルや変換用ソースの再生成などを一括で行い、プロジェクト全体の一貫性を保ちます。

## Web Toolchain / WebAssembly Toolchain

- **Web Toolchain**
  - **役割**: `ebmgen`, `ebmcodegen`などのコアツール群をWebAssemblyにコンパイルし、ブラウザ上で実行可能にしたもの。
  - **場所**: `web/tool` ディレクトリに、コンパイル済みの `.wasm` ファイルや関連するJavaScriptの `glue` コードが配置されます。
  - **生成プロセス**: 
    - **Emscripten/CMake**: WebAssemblyへのコンパイルそのものと、それらを直接呼び出すための `glue` コードの生成は、`CMake` に設定された `Emscripten` ツールチェーンによって行われます。
    - **ebmwebgen.py**: Emscriptenが生成したWASMモジュールとWebフロントエンドのUIとを繋ぐ、JavaScriptの「グルーコード」を生成するスクリプト。各ツールのコマンドラインフラグを`--show-flags`で自己申告させ、それを基にUIの動的生成やWebWorkerとの通信を行うコードを自動生成します。

## 高度なツールと機能

- **ebmgen Interactive Mode**

  - **役割**: `ebmgen`に搭載された対話型のデバッガおよびシェル機能。
  - **場所**: `src/ebmgen/interactive/` に実装があります。
  - **機能**: EBM ファイルを読み込み、クエリ言語を用いて EBM の内部構造を対話的に調査・デバッグすることができます。

- **EBM Transformation**

  - **役割**: `ebmgen`が EBM に対して行う最適化や構造変換の機能。
  - **場所**: `src/ebmgen/transform/` に実装があります。
  - **例**: 不要な要素の削除や、構造の単純化などが含まれます。

- **ebmdsl.py (実装)**

  - **役割**: `ebmcodegen`の内部で使われる DSL の実装。
  - **場所**: `src/ebmcodegen/dsl/` に実装があります。
  - **詳細**: コード生成のロジックをより簡潔に記述するために、`ebmcodegen`の内部で使用される特殊な構文や機能を提供します。

- **ebm2jsonschema.py**

  - **役割**: EBM の構造定義を JSON Schema 形式に変換するツール。
  - **用途**: EBM の構造を他のツールやシステムと連携させる際のスキーマ定義として利用できます。

- **unictest_setup.py**
  - **役割**: `unictest.py`フレームワークのセットアップを行うスクリプト。
  - **詳細**: テストの実行に必要な設定や環境を準備します。`unictest.py`と密接に関連して動作します。

## ジェネレータ実装の詳細

- **default_codegen_visitor**

  - **役割**: `ebmcodegen`が提供する、デフォルトのビジター実装。
  - **場所**: `src/ebmcodegen/default_codegen_visitor/`
  - **用途**: 言語ジェネレータ開発者が独自に処理を実装しない場合、このデフォルト実装が使われ、EBM のツリー構造を再帰的に辿ります。

- **stub**

  - **役割**: `ebmcodegen`によって生成されるスケルトンコードに含まれる、共通のヘルパーコード。
  - **場所**: `src/ebmcodegen/stub/`
  - **内容**: エントリーポイントの定義など、ジェネレータの定型的な部分を実装しています。

- **structs.cpp (`ebmcodegen`)**

  - **役割**: `ebmcodegen`のコア機能であるリフレクション（自己解析）ロジックの実装。
  - **場所**: `src/ebmcodegen/structs.cpp`
  - **動作**: EBM の C++ヘッダファイルを解析し、その構造（struct や enum）を抽出して、コード生成のスケルトンやメタデータを生成します。

- **debug_printer.cpp (`ebmgen`)**

  - **役割**: EBM の内容を人間が読める形式でテキスト出力するデバッグ機能の実装。
  - **場所**: `src/ebmgen/debug_printer.cpp`

- **json_printer.cpp (`ebmgen`)**
  - **役割**: EBM を JSON 形式でシリアライズ（出力）する機能の実装。
  - **場所**: `src/ebmgen/json_printer.cpp`

## レガシーワークフロー

- **script/bm/**
  - **役割**: `ebmgen`や`ebmcodegen`に置き換えられる前の、古い`bmgen`や`bm2`（BinaryModule ベース）のツールチェーンで使われていたスクリプト群。
  - **位置づけ**: 現在の開発では積極的には使用されませんが、プロジェクトの歴史的経緯を理解する上で参考になります。
