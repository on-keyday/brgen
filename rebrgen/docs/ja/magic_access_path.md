# EBM マジックアクセスパス API リファレンス

このドキュメントでは、EBM (Extended Binary Module) データ構造をトラバースするために使用できる有効な「マジックアクセスパス」の概要を説明します。これらのパスは、参照の逆参照とフォールバックを自動的に処理することで、ネストされたデータにアクセスする便利な方法を提供します。

## アクセスルール

アクセスロジックは、以下のルールに従います。

1.  **暗黙的な逆参照**: `StatementRef`、`TypeRef`、および `ExpressionRef` は、対応するインスタンス (`Statement`、`Type`、`Expression`) に自動的に逆参照されます。`IdentifierRef` のような他の `Ref` 型は逆参照されません。
2.  **名前解決のフォールバック**: ルート型 (`Statement`、`Type`、`Expression`) でフィールドが見つからない場合、システムは自動的にその型の `body` メンバー内でフィールドを検索しようとします。たとえば、`Statement.kind` は `Statement.body.kind` として解決されます。
3.  **バリアント/共用体のトラバーサル**: 上記のルールの組み合わせにより、バリアント型内のフィールドに直接アクセスできます (例: `Statement` オブジェクトから `var_decl` フィールドに直接アクセスするなど)。
4.  **インスタンスアクセス (`.instance`)**: 特別な `.instance` キーワードは、参照が指すオブジェクトのメンバーにアクセスするのではなく、オブジェクト自体を取得するために使用できます。これは、逆参照されたオブジェクト自体を取得するのに役立ちます。

## アクセスパスリファレンス

### インスタンスアクセス (`.instance`)

`.instance` キーワードは、参照から基になるオブジェクトを取得する方法を提供します。

| ルート型     | アクセスパスの例      | 解決される型 | 使用されるロジック                | 説明                                                                                               |
| :----------- | :-------------------- | :------------- | :-------------------------------- | :------------------------------------------------------------------------------------------------- |
| `Statement`  | `id.instance`         | `Statement*`   | Ref(Stmt) + Instance              | `id` (`StatementRef`) が逆参照された後、`.instance` は `Statement` オブジェクト自体を返します。       |
| `Expression` | `type.instance`       | `Type*`        | フォールバック + Ref(Type) + Instance | `type` (`TypeRef`) が逆参照された後、`.instance` は `Type` オブジェクト自体を返します。             |
| `Type`       | `body.id.instance`    | `Statement*`   | 直接 + Ref(Stmt) + Instance     | `STRUCT` または `ENUM` 型の場合、`body.id` (`StatementRef`) を逆参照した後、`.instance` は `Statement` オブジェクト自体を返します。 |

### ルート型: `Statement`

| ルート型     | アクセスパスの例                 | 解決される型    | 使用されるロジック             | 説明                                             |
| :----------- | :------------------------------- | :-------------- | :----------------------------- | :----------------------------------------------- |
| `Statement`  | `id`                             | `StatementRef`  | 直接                         | ステートメントの一意の ID にアクセスします。     |
| `Statement`  | `body`                           | `StatementBody` | 直接                         | ステートメントの主要なボディ共用体にアクセスします。 |
| `Statement`  | `kind`                           | `StatementKind` | フォールバック                   | `body.kind` にアクセスして、特定のステートメント型を取得します。 |
| `Statement`  | `var_decl`                       | `VariableDecl`  | フォールバック                   | 変数宣言の `body.var_decl` にアクセスします。    |
| `Statement`  | `var_decl.name`                  | `IdentifierRef` | フォールバック                   | 宣言された変数の名前にアクセスします。逆参照されません。 |
| `Statement`  | `var_decl.var_type`              | `TypeRef`       | フォールバック                   | 宣言された変数の型参照にアクセスします。         |
| `Statement`  | `struct_decl.fields`             | `Block`         | フォールバック                   | 構造体宣言内のフィールドのブロックにアクセスします。 |
| `Statement`  | `struct_decl.fields.0`           | `StatementRef`  | フォールバック + インデックス        | 構造体内の最初のフィールドステートメント (`FieldDecl`) にアクセスします。 |
| `Statement`  | `func_decl.return_type`          | `TypeRef`       | フォールバック                   | 関数宣言の戻り値の型にアクセスします。           |
| `Statement`  | `func_decl.return_type.kind`     | `TypeKind`      | フォールバック + Ref(Type)     | 戻り値の型の種類 (`Type.body.kind`) にアクセスします。 |
| `Statement`  | `if_statement.then_block`        | `StatementRef`  | フォールバック                   | if ステートメントの 'then' ブロックにアクセスします。 |
| `Statement`  | `if_statement.then_block.kind`   | `StatementKind` | フォールバック + Ref(Stmt)     | 'then' ブロック内のステートメントの種類にアクセスします。 |

### ルート型: `Expression`

| ルート型     | アクセスパスの例                     | 解決される型    | 使用されるロジック             | 説明                                           |
| :----------- | :----------------------------------- | :-------------- | :----------------------------- | :--------------------------------------------- |
| `Expression` | `id`                                 | `ExpressionRef` | 直接                         | 式の一意の ID にアクセスします。               |
| `Expression` | `type`                               | `TypeRef`       | フォールバック                   | 式の結果の型参照である `body.type` にアクセスします。 |
| `Expression` | `type.kind`                          | `TypeKind`      | フォールバック + Ref(Type)     | 式の型の種類 (`Type.body.kind`) にアクセスします。 |
| `Expression` | `kind`                               | `ExpressionKind`| フォールバック                   | `body.kind` にアクセスして、特定の式型を取得します。 |
| `Expression` | `bop`                                | `BinaryOp`      | フォールバック                   | `BINARY_OP` の場合、`body.bop` にアクセスして演算子を取得します。 |
| `Expression` | `bop.left`                           | `ExpressionRef` | フォールバック                   | `BINARY_OP` の場合、左側のオペランドである `body.left` にアクセスします。 |
| `Expression` | `bop.left.kind`                      | `ExpressionKind`| フォールバック + Ref(Expr)     | 左側のオペランド式の種類にアクセスします。       |
| `Expression` | `identifier`                         | `StatementRef`  | フォールバック                   | `IDENTIFIER` 式の場合、宣言への `StatementRef` である `body.id` にアクセスします。 |
| `Expression` | `identifier.kind`                    | `StatementKind` | フォールバック + Ref(Stmt)     | 識別子を宣言したステートメントの種類にアクセスします。 |
| `Expression` | `identifier.var_decl.name`           | `IdentifierRef` | フォールバック + Ref(Stmt)     | この識別子の変数宣言の名前にアクセスします。     |
| `Expression` | `call_desc.callee`                   | `ExpressionRef` | フォールバック                   | 関数呼び出しの呼び出し先式にアクセスします。     |
| `Expression` | `call_desc.arguments.0`              | `ExpressionRef` | フォールバック + インデックス        | 関数呼び出しの最初の引数にアクセスします。       |

### ルート型: `Type`

| ルート型 | アクセスパスの例                 | 解決される型    | 使用されるロジック             | 説明                                                              |
| :------- | :------------------------------- | :-------------- | :----------------------------- | :---------------------------------------------------------------- |
| `Type`   | `id`                             | `TypeRef`       | 直接                         | 型の一意の ID にアクセスします。                                  |
| `Type`   | `body`                           | `TypeBody`      | 直接                         | 型の主要なボディ共用体にアクセスします。                          |
| `Type`   | `kind`                           | `TypeKind`      | フォールバック                   | `body.kind` にアクセスして、特定の型種別を取得します。            |
| `Type`   | `size`                           | `Varint`        | フォールバック                   | `INT` または `UINT` の場合、`body.size` にアクセスしてビット単位のサイズを取得します。 |
| `Type`   | `element_type`                   | `TypeRef`       | フォールバック                   | `ARRAY` または `VECTOR` の場合、`body.element_type` にアクセスします。 |
| `Type`   | `element_type.kind`              | `TypeKind`      | フォールバック + Ref(Type)     | 配列/ベクター要素の型の種類にアクセスします。                     |
| `Type`   | `body.id`                        | `StatementRef`  | 直接                         | `STRUCT` または `ENUM` 型の場合、宣言への `StatementRef` にアクセスします。 |
| `Type`   | `body.id.kind`                   | `StatementKind` | 直接 + Ref(Stmt)             | 参照される宣言の種類 (例: `STRUCT_DECL`) にアクセスします。       |
| `Type`   | `body.id.struct_decl.fields`     | `Block`         | 直接 + Ref(Stmt)             | 参照される構造体宣言のフィールドにアクセスします。                |
| `Type`   | `variant_desc.members`           | `Types`         | フォールバック                   | `VARIANT` 型のメンバー型のリストにアクセスします。                  |
| `Type`   | `variant_desc.members.0`         | `TypeRef`       | フォールバック + インデックス        | `VARIANT` の最初のメンバー型にアクセスします。                    |
