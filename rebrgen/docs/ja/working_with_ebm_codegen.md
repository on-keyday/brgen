## 3. EBM とコードジェネレーターの操作 (ebmgen/ebmcodegen)

### 3.1 `ebmgen`の実行

`ebmgen`は様々な入力形式 (brgen JSON AST、`.bgn`ファイル、EBM バイナリ) を受け取り、EBM ファイルを生成できます。デバッグおよびクエリ機能をサポートしています。

**基本的な変換:**

- `brgen` AST JSON を EBM バイナリファイルに変換: `./tool/ebmgen -i <path/to/input.json> -o <path/to/output.ebm>`
- `.bgn`ファイルを直接変換 (`libs2j`が必要): `./tool/ebmgen -i <path/to/input.bgn> -o <path/to/output.ebm>`

**デバッグ:**

- EBM の人間が読めるテキストダンプを出力: `./tool/ebmgen -i <path/to/input.ebm> -d <path/to/debug_output.txt>`
- EBM 構造を JSON ファイルとして出力: `./tool/ebmgen -i <path/to/input.ebm> -d <path/to/debug_output.json> --debug-format json`

**共通コマンドラインオプション:**
| フラグ | エイリアス | 説明 |
| ---------------- | ----- | ----------------------------------------------------------------------------------------------------------------------- |
| `--input` | `-i` | **(必須)** 入力ファイルを指定します。 |
| `--output` | `-o` | 出力 EBM バイナリファイルを指定します。`-`を指定すると標準出力に出力します。 |
| `--debug-print` | `-d` | デバッグ出力ファイルを指定します。`-`を指定すると標準出力に出力します。 |
| `--input-format` | | 入力形式を明示的に設定します。`bgn`、`json-ast`、`ebm`のいずれか。デフォルトはファイル拡張子に基づいて自動検出されます。 |
| `--debug-format` | | デバッグ出力の形式を設定します。`text` (デフォルト)、`json`のいずれか。 |
| `--interactive` | `-I` | 入力ファイルロード後にインタラクティブデバッガを開始します。 |
| `--query` | `-q` | コマンドラインから直接クエリを実行します。 |
| `--query-format` | | `--query`フラグの出力形式を設定します。`id` (デフォルト)、`text`、`json`のいずれか。 |
| `--cfg-output` | `-c` | 制御フローグラフ (CFG) を指定されたファイルに出力します。 |
| `--libs2j-path` | | `.bgn`ファイルを変換するための`libs2j`ダイナミックライブラリへのパスを指定します。 |
| `--debug` | `-g` | デバッグ変換を有効にします (EBM から未使用のアイテムを削除しないなど)。 |
| `--verbose` | `-v` | 詳細なログ出力を有効にします (デバッグ用)。 |
| `--timing` | | 各主要ステップの処理時間を表示します。 |
| `--base64` | | base64 エンコーディングで出力します (Web プレイグラウンド互換性のため)。 |
| `--output-format`| | 出力形式 (デフォルト: バイナリ)。 |
| `--show-flags` | | コマンドラインフラグの説明を JSON 形式で出力します。 |

### 3.2 `ebmcodegen`の実行

`ebmcodegen`はコードジェネレータージェネレーターです。その主な用途は、新しい言語固有のコードジェネレーターの C++テンプレートを作成することです。

- "my_lang"という名前の言語の新しいジェネレーターテンプレートを作成します: `python ./script/ebmcodegen.py my_lang`
  このコマンドは`./tool/ebmcodegen`を利用し、`src/ebmcg/ebm2my_lang/`に新しいジェネレーターのスケルトンを含む新しいディレクトリを作成します。

**共通コマンドラインオプション:**
| フラグ | エイリアス | 説明 |
| -------------------------------------------------------------------- | ----- | ----------------------------------------------------------------------------------------------------------------------- |
| `--help, -h` | | このヘルプを表示します |
| `-l=LANG, --lang=LANG` | | 出力言語 |
| `-p=PROGRAM_NAME, --program-name=PROGRAM_NAME` | | プログラム名 (デフォルト: ebm2{lang}) |
| `-d=DIR, --visitor-impl-dir=DIR` | | ビジター実装のディレクトリ |
| `--visitor-impl-dsl-dir=DIR` | | DSL から生成されるビジター実装のディレクトリ |
| `--default-visitor-impl-dir=DIR` | | デフォルトのビジター実装のディレクトリ |
| `--template-target=target_name` | | テンプレートターゲット名。`--mode hooklist`を参照 |
| `--dsl-file=FILE` | | `--mode dsl`用の DSL ソースファイル |
| `--mode={subset,codegen,interpret,hooklist,hookkind,template,spec-json,dsl}` | | 生成モード (デフォルト: codegen) |

### 3.3 `script/update_ebm.py`による EBM 構造の更新

`script/update_ebm.py`スクリプトは、`extended_binary_module.bgn`構造が変更されたときに EBM 関連ファイルを再生成するプロセス全体を自動化します。これにより、プロジェクト全体の一貫性が保証されます。スクリプトは以下の操作を実行します。

1.  **C++ EBM 定義の更新**: `src/ebm/ebm.py`を実行して、`src/ebm/extended_binary_module.bgn`から`src/ebm/extended_binary_module.hpp`と`src/ebm/extended_binary_module.cpp`を再生成します。
2.  **初期ビルド**: `script/build.py`を実行して、更新された EBM C++定義でプロジェクトをビルドします。
3.  **`body_subset.cpp`の生成**: `tool/ebmcodegen --mode subset`を使用して、EBM 構造に関するメタデータを含む`src/ebmcodegen/body_subset.cpp`を作成します。
4.  **JSON 変換ヘッダーの生成**: `tool/ebmcodegen --mode json-conv-header`を使用して、JSON デシリアライズ用の`src/ebmgen/json_conv.hpp`を作成します。
5.  **JSON 変換ソースの生成**: `tool/ebmcodegen --mode json-conv-source`を使用して、JSON デシリアライズ用の`src/ebmgen/json_conv.cpp`を作成します。
6.  **条件付き再ビルド**: 生成されたファイル (ステップ 3-5) のいずれかが変更された場合、スクリプトは`script/build.py`を再度実行して、すべてのツールが最新であることを確認します。
7.  **Hex テストデータの生成**: `src/ebm/extended_binary_module.bgn`を 16 進形式に変換し、テストに使用できる`test/binary_data/extended_binary_module.dat`に保存します。
    EBM 構造とすべての依存する生成ファイルを更新するには、単に`python script/update_ebm.py`を実行します。
