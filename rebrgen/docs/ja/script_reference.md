## 7. スクリプト参照

### ビルド関連

- **`build.py` / `build.ps1` / `build.bat` / `build.sh`**: プロジェクトのビルドスクリプト。`build.py`は C++ビルド (`native`、`web`モード) のために`cmake`と`ninja`を管理します。その他はラッパーです。

### コード生成とテンプレート関連

- **`ebmcodegen.py`**: `tool/ebmcodegen`をラップし、`src/ebmcg/ebm2<lang>`に新しい EBM ベースのコードジェネレーター/インタプリター用のスケルトンコード (`main.cpp`、`CMakeLists.txt`) を生成します。
- **`ebmtemplate.py`**: `ebmcodegen`を使用してビジターフックテンプレートファイル (作成、コメント更新、テスト生成) を管理するためのヘルパースクリプトです。
- **`ebmwebgen.py`**: EBM ベースのコードジェネレーターを Web アプリケーションとして実行するための JavaScript グルーコード (例: `web/tool`内の`ebm_caller.js`) を自動生成します。
- **`script/ebmdsl.py`**: `.dsl`ファイルを C++ビジターフック実装にコンパイルするプロセスを自動化します。`src/ebmcg/`内の言語ディレクトリを反復処理し、その`dsl`サブディレクトリで見つかった`.dsl`ファイルを`tool/ebmcodegen --mode dsl`を使用して処理し、生成された C++コードを`visitor/dsl`内の対応する`.hpp`ファイルに書き込みます。
- **`src/ebm/ebm.ps1`**: (`src/ebm`にあります) `extended_binary_module.bgn`から`extended_binary_module.hpp`と`extended_binary_module.cpp`を生成する PowerShell スクリプトです。`src/ebm/ebm.py`と同様の機能です。

### テストと実行関連

- **`ebmtest.py`**: `ebmgen`の EBM JSON 出力をスキーマとテストケースの期待値に対して検証します。
- **`unictest.py`**: `ebmgen`、コードジェネレーター、およびテストスクリプトを実行することで、自動テストおよび開発ワークフローを調整します (セクション 5.1 を参照)。
  - `--target-runner <runner_name>`: `unictest_runner.json`ファイルで定義されているターゲットランナーの名前 (例: `ebm2rust`) を指定します。複数回指定できます。
  - `--target-input <input_name>`: `test/inputs.json`で定義されている入力の名前 (例: `websocket_frame_valid`) を指定します。複数回指定できます。

### ユーティリティ

- **`split_dot.py`**: 大規模な DOT グラフファイル (`ebmgen`によって生成) をサブグラフとして個別の DOT ファイルと SVG ファイルに分割し、視覚化を向上させます。

### 旧世代スクリプト (歴史的背景のための参照であり、現在の使用ではありません)

- **`gen_template.py` / `gen_template.ps1` / `gen_template.bat` / `gen_template.sh`**: `bmgen`用。`src/old/bm2<lang>/`に言語コードジェネレーターテンプレート (`.hpp`、`.cpp`、`main.cpp`、`CMakeLists.txt`) を生成します。
- **`generate.py` / `generate.ps1` / `generate.bat` / `generate.sh`**: `bmgen`用。コード生成と設定更新全体を調整します (例: `gen_template.py`、`collect_cmake.py`、`generate_test_glue.py`)。
- **`collect_cmake.py`**: `bmgen`用。`bm2<lang>`ディレクトリの`add_subdirectory`を追加することで、トップレベルの`src/CMakeLists.txt`を自動生成し、`run_generated.py`を生成します。
- **`generate_web_glue.py`**: `bmgen`用。Web アプリケーション用の JavaScript グルーコードを生成します。
- **`run_generated.py` / `run_generated.ps1` / `run_generated.bat` / `run_generated.sh`**: `bmgen`用。`collect_cmake.py`によって自動ビルドされます。プロジェクトをビルドし、`.bgn`を JSON AST に変換し、`bmgen`を実行し、その後言語ジェネレーターを実行します。
- **`run_cmptest.py` / `run_cmptest.ps1` / `run_cmptest.bat` / `run_cmptest.sh`**: `brgen`用。`testkit/`の設定に基づいて、生成されたコードの互換性テストのために`cmptest`ツールを実行します。
- **`generate_test_glue.py`**: `bmgen`用。`testkit/`に`cmptest`で使用されるテスト設定ファイル (`cmptest.json`、`test_info.json`など) を生成します。
- **`generate_golden_master.py`**: `bmgen`用。`gen_template`を使用してすべての言語ジェネレーターソースコードを生成し、「ゴールデンマスター」テストのために`test/golden_masters/`に保存します。
- **`test_compatibility.py`**: `bmgen`用。`gen_template`の現在の出力と、`generate_golden_master.py`で作成されたゴールデンマスターを比較します。これにより、テンプレート生成ロジックの変更が既存の出力に影響を与えていないか (後方互換性があるか) をテストします。
- **`binary_module.bat` / `binary_module.ps1`**: `bmgen`用。`brgen`の`src2json`と`json2cpp2`を使用して、`binary_module.bgn`から C++ヘッダーとソースファイル (`binary_module.hpp`、`binary_module.cpp`) を生成します。
- **`list_lang.py`**: `bmgen`用。`src/`内の`bm2<lang>`ディレクトリを探索して、サポートされている言語のリストを JSON 形式で出力します。
- **`run_cycle.py` / `run_cycle.ps1` / `run_cycle.bat` / `run_cycle.sh`**: 旧世代。開発サイクル全体を実行します: `generate.py`、`run_generated.py`、`run_cmptest.py`。
