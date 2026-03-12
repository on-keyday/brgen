## 1. プロジェクト概要

### 1.1 プロジェクトの目標と目的
`rebrgen`プロジェクトは、`brgen` (https://github.com/on-keyday/brgen) のためのジェネレーター構築プロジェクトです。オリジナルの`brgen`コードジェネレーターがAST-to-Codeモデルを使用しているのに対し、`rebrgen`は柔軟性と多言語互換性の向上のため、AST-to-IR-to-Codeモデルを採用しています。
このサブプロジェクトである`ebmgen`はIRプロジェクトの一部であり、`bmgen`および`BinaryModule(bm)`の後継です。`brgen`の抽象構文木 (AST) を、より優れた中間表現 (IR) である**`ExtendedBinaryModule` (EBM)**に変換します。
`ebmcodegen`サブプロジェクトはコードジェネレータージェネレーターであり、ビジターパターンに基づくリフレクションメカニズムで`ExtendedBinaryModule`をスキャンし、コードジェネレーターのバックボーンを形成するC++コードを生成します。

### 1.2 ワークフローの概要
全体のワークフローは以下の通りです。
1.  `.bgn`ファイルは`src2json` (`brgen`リポジトリから) によって処理され、`brgen` AST (JSON) を生成します。
2.  `brgen` AST (JSON) は`ebmgen`に入力され、EBMを生成します。
3.  EBMはその後、言語固有のコードジェネレーター (`ebmcodegen`を使用して構築) によって処理され、様々な言語のコードを生成します。
4. `.bgn`ファイルが`extended_binary_module.bgn`自体である場合、`json2cpp2` (`brgen`リポジトリから) は`extended_binary_module.hpp`/`.cpp`ファイルを生成し、これらは`ebmcodegen`と統合されてコードジェネレーターのC++ソースコードを生成します。

### 1.3 EBM: 優れた中間表現
EBM (`src/ebm/extended_binary_module.bgn`) は、オブジェクト (Statement、Expression、Typeなど) が一意のIDで参照される集中型データテーブルを持つグラフベースの構造を特徴としています。構造化された制御フロー (if、loop、match)、高レベルの抽象化 (「どのように」ではなく「何を」に焦点を当てる)、低レベル表現への橋渡しとなる低レベルステートメントをサポートし、エンディアンやビットサイズなどの重要なバイナリ形式の詳細を保持します。
