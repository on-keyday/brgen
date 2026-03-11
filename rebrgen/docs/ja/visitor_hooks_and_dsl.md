## 4. コード生成ロジック (ビジターフック)

### 4.1 ビジターフックの概要
ビジターフックは、特定のEBMノード（例: `Statement_WRITE_DATA`）のコード生成ロジックです。
最新の**クラスベースフックシステム**では、ロジックは生成されたコンテキストクラスのメソッドとして実装されます。このシステムは、完全なIDEサポート（自動補完）と型安全性を提供します。

-   **構造**: `DEFINE_VISITOR(HookName)`マクロを使用してビジターロジックを定義します。
-   **コンテキスト**: 必要なすべてのデータ（EBMフィールド、ヘルパー関数、ビジターインスタンス）は、単一の`ctx`オブジェクトを介してアクセスできます。
-   **戻り値の型**: フックは`expected<Result>`を返さなければなりません。`Result`には、生成されたコードを保持する`CodeWriter`オブジェクトが含まれています。

*注: フックが単純な`.hpp`ファイルであり、ジェネレーターに直接インクルードされるレガシーな「直接インクルード」システムも存在します。これは後方互換性のためにサポートされていますが、新しい実装では**非推奨**です。*

### 4.2 `ebmtemplate.py`によるビジターフックの管理
`script/ebmtemplate.py`スクリプトは、`ebmcodegen`をラップし、ビジターフックのライフサイクルを簡素化します。
-   **`python script/ebmtemplate.py`**: ヘルプを表示します。
-   **`python script/ebmtemplate.py interactive`**: スクリプト機能のインタラクティブガイドを開始します。
-   **`python script/ebmtemplate.py <template_target>`**: フックファイルの概要を標準出力に出力します。
-   **`python script/ebmtemplate.py <template_target> <lang>`**: `src/ebmcg/ebm2<lang>/visitor/`に新しいフックファイル (`.hpp`) を生成します。**デフォルトでは、クラスベースのフックが生成されます。**
-   **`python script/ebmtemplate.py update <lang>`**: 指定された言語の既存のすべてのフックの自動生成されたコメントブロック (利用可能な変数リスト) を更新します。
-   **`python script/ebmtemplate.py test`**: 利用可能なすべてのテンプレートターゲットの生成をテストします。
-   **`python script/ebmtemplate.py list <lang>`**: 指定された[lang]ディレクトリ内のすべての定義済みテンプレートをリストします。

### 4.3 ビジターフック実装のワークフロー
1.  **ターゲットの検索**: 処理したいEBMノードを決定します。`tool/ebmcodegen --mode hooklist`を使用して、利用可能なフックをリストします。
2.  **ファイルの作成**: `python script/ebmtemplate.py <template_target> <lang>`を使用してフックファイルを生成します。
    *   例: `python script/ebmtemplate.py Statement_WRITE_DATA rust` は `src/ebmcg/ebm2rust/visitor/Statement_WRITE_DATA_class.hpp` を作成します。
3.  **ロジックの実装**: 新しく作成されたファイルを開きます。`DEFINE_VISITOR`ブロック内の`/*here to write the hook*/`コメントを実装に置き換えます。データにアクセスするには`ctx`オブジェクトを使用します。
4.  **ビルドと検証**: フックを実装した後、`python script/unictest.py` (またはデバッグ用に`python script/unictest.py --print-stdout`) を実行して、変更をビルドおよび検証します。これにより、ビルドプロセスがトリガーされ、テストが実行されます。`unictest.py`は、適切な場合に`--debug-unimplemented`フラグをコードジェネレーターに渡して、未実装のフックを特定するのに役立ちます。

### 4.4 ビジターフックAPIリファレンス

ビジターフックは、`DEFINE_VISITOR`マクロ内で実装されます。

#### 4.4.1 戻り値の型と`Result`構造体

すべてのビジターフックは`expected<Result>`型の値を返さなければなりません。

-   **`expected<T>`**: エラーが発生する可能性のある操作の結果を表す型です。エラーハンドリングのために`MAYBE`マクロと組み合わせて使用されます。
-   **`Result`構造体**:
    -   `CodeWriter value`: 生成されたコード文字列を保持します。フックの目的は、このオブジェクト内にターゲット言語のコードを構築することです。

#### 4.4.2 利用可能な変数 (`ctx`)

クラスベースシステムでは、すべての変数は**`ctx`**オブジェクトを介してアクセスされます。
`ctx`の利用可能なメンバーは、フックファイルの先頭にある自動生成されたコメントブロックにリストされています。

-   **EBMフィールド**: 訪問しているノードのフィールドに直接アクセスします (例: `ctx.io_data`, `ctx.endian`)。
-   **ヘルパーアクセサ**: 例: `ctx.identifier()` でステートメントに関連付けられた名前を取得します。
-   **ビジターインスタンス**: `ctx.visitor` でメインのビジターオブジェクトにアクセスできます (ただし、通常は `ctx.visit` ラッパーを使用します)。

-   **変数の更新**: EBM構造が変更された場合は、`python script/ebmtemplate.py update <lang>`を実行して、コメントブロックを更新してください。

#### 4.4.3 ヘルパー関数とマクロ

-   **`MAYBE(var, expr)`**: エラーハンドリングマクロ。`expr`がエラーを返した場合、それを伝播して早期リターンします。成功した場合は、値が`var`に代入されます。
    -   **使用例**: `MAYBE(res, ctx.visit(ctx.some_expr));`
-   **`ctx.visit(ref)`**: `ref`で参照されるEBMオブジェクト (Statement, Expression, Typeなど) を再帰的に訪問し、生成されたコード (`expected<Result>`) を返します。
    -   **使用例**: `MAYBE(code, ctx.visit(ctx.stmt.body));`
-   **`std::format`**: C++20の文字列フォーマット機能。
-   **`to_string(enum_value)`**: EBMの列挙型 (例: `SizeUnit`, `Endian`) の値を文字列に変換します。

#### 4.4.4 `extended_binary_module.hpp`の参照

EBMの構造体、列挙型、および参照型 (`StatementRef`, `ExpressionRef`, `TypeRef`など) の正確な定義については、`src/ebm/extended_binary_module.hpp`ファイルを参照してください。このファイルは、`extended_binary_module.bgn`から自動生成されます。

### 4.5 DSL (ドメイン固有言語) for ビジターフック
`ebmcodegen`ツールは、ビジターフックを記述するためのドメイン固有言語 (DSL) をサポートしており、コード生成ロジックをより簡潔で読みやすい方法で定義できます。このDSLは、`--mode dsl`および`--dsl-file=FILE`で呼び出されたときに`ebmcodegen`によって処理されます。

**DSL構文の概要:**
DSLは、特別なマーカーを使用してC++コード、EBMノード処理、および制御フロー構造を混在させます。
DSLの具体的な使用例については、`src/ebmcg/ebm2python/dsl_sample/`を参照してください。
-   **`{% C++_CODE %}`**: C++リテラルコードを生成された出力に直接埋め込みます。(例: `{% int a = 0; %}`)
-   **`{{ C++_EXPRESSION }}`**: 結果が出力に書き込まれるC++式を埋め込みます。(例: `{{ a }} += 1;`)
-   **`{* EBM_NODE_EXPRESSION *}`**: `visit_Object`を呼び出し、生成された出力を書き込むことでEBMノード (例: `ExpressionRef`、`StatementRef`) を処理します。(例: `{* expr *}`)
-   **`{& IDENTIFIER_EXPRESSION &}`**: 識別子を取得し、それを出力に書き込みます。(例: `{& item_id &}`)
-   **`{! SPECIAL_MARKER !}`**: DSL自体内の高度な制御フローおよび変数定義に使用されます。これらのマーカー内のコンテンツは、ネストされたDSLによって解析されます。
    -   **`transfer_and_reset_writer`**: 現在の`CodeWriter`コンテンツを転送し、リセットするC++コードを生成します。
    -   **`for IDENT in (range(BEGIN, END, STEP) | COLLECTION)`**: C++ `for`ループを生成します。数値範囲とコレクションのイテレーションの両方をサポートします。
    -   **`endfor`**: `for`ループブロックを閉じます。
    -   **`if (CONDITION)` / `elif (CONDITION)` / `else`**: C++ `if`/`else if`/`else`ブロックを生成します。
    -   **`endif`**: `if`ブロックを閉じます。
    -   **`VARIABLE := VALUE`**: 生成されたコード内でC++変数を定義します。(例: `my_var := 42`)

これらのマーカーの外側のテキストはターゲット言語コードとして扱われ、出力に書き込まれる前にエスケープされます。DSLは、ソースの書式設定に基づいて自動インデントとデデントも処理します。
