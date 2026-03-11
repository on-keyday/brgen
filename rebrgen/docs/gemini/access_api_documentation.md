# Role

あなたは EBM (Extended Binary Module) のテクニカルライターです。

# Instructions

以下の手順で、開発者がコード生成時に使用する「マジック文字列パス（Magic Access Path）」の API リファレンスを作成してください。

## Step 1: Read Context

以下のファイルを読み込み、データ構造とアクセサの仕様を理解してください。

1. **Schema Definition:** `./src/ebm/extended_binary_module.bgn`

   - データ構造（Format, Enum, Union）の定義元です。これを正とします。

2. **Traversal Logic:** `./src/ebmgen/access.hpp`
   - パス解決のコアロジック（`FieldAccessor` 等）が実装されているファイルです。
   - ※内部で include されている `access_helper.hpp` は自動生成コードなので、ロジックの解析には `access.hpp` を重点的に参照してください。

## Step 2: Understand the "Magic" Rules (Strictly)

`access.hpp` の実装は、以下の挙動をコンパイル時に実現しています。
コードを読む際は、以下の解釈ルール（特に **Rule A** の適用範囲）を前提にしてください。

1.  **Rule A: Implicit Dereference (特定の参照型の自動解決)**

    - **対象:** `StatementRef`, `TypeRef`, `ExpressionRef` の **3 つの型のみ**。
    - **挙動:** これらの型に遭遇した場合、自動的に ID を解決して実体 (`Statement`, `Type`, `Expression`) へのアクセスとして扱います（`ref->member` のように振る舞う）。
    - **除外:** `StringRef` や `IdentifierRef` は **自動解決されません**。これらは単なる終端（ID または値）として扱ってください。これらに対してさらにドット記法でプロパティアクセスすることはできません。

2.  **Rule B: Name Resolution Fallback (Body 経由の自動探索)**

    - ある型 `T` にフィールド `X` が存在しない場合、`T.body` フィールドが存在すれば、自動的に `T.body.X` を探索します。
    - 例: `Statement` 型には `kind` フィールドはありませんが、`Statement.body.kind` が存在するため、パス `Statement.kind` は有効です。

3.  **Variant/Union Traversal**
    - 上記の挙動の組み合わせにより`StatementRef`からアクセスすると`StatementBody` や `ExpressionBody` 内の具体的な型（例: `ParameterDecl`）が持つフィールドへも、パス上は直接アクセスできます。
    - 例: access_field<`param_decl`>(StatementRef{}) == access_field<`body.param_decl`>(StatementRef{})

# Task

`Statement`, `Expression`, `Type` の 3 つをルート（Root）として、有効な「アクセスパス（Access Path）」と「解決される型（Resolved Type）」のリストを Markdown の表形式で./docs ディレクトリ内にドキュメントを書いてください。

**制約:**

- 実際のスキーマ定義 (`.bgn`) に基づいて正確な型名を記述すること。
- ドット(`.`)で区切られた階層が **深さ 3 階層以内** のものを網羅的に（または代表的なものを）リストアップすること。
- `StringRef` や `IdentifierRef` の先へ進むパス（例: `name.body`）を出力しないこと。

# Output Format

| Root Type    | Access Path Example  | Resolved Type   | Logic Used               | Description                                                                                             |
| :----------- | :------------------- | :-------------- | :----------------------- | :------------------------------------------------------------------------------------------------------ |
| `Statement`  | `body`               | `StatementBody` | Direct                   | Explicit access to body                                                                                 |
| `Statement`  | `kind`               | `StatementKind` | **Fallback**             | Implicit access via `Statement.body`                                                                    |
| `Expression` | `body.id.param_decl` | `ParameterDecl` | **Ref(Stmt) + Fallback** | `Expression.body` -> `id` (StatementRef) -> `[Ref]` -> `Statement` -> `[Fallback]` -> `body.param_decl` |
| `Statement`  | `var_decl`           | `VariableDecl`  | **Ref(Stmt) + Fallback** | Accessing specialized field inside VariableDecl                                                         |
