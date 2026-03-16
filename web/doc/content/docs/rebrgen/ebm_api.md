---
title: "EBM API Reference"
weight: 6
---

{{< hint info >}}
このドキュメントは AI (Claude) によって `rebrgen/docs/en/magic_access_path.md` を元に生成されたものです。内容に誤りがある場合は [GitHub Issue](https://github.com/on-keyday/brgen/issues) で報告してください。
{{< /hint >}}

# EBM API Reference

このページでは以下の2つの異なる API を説明します:

| API | 用途 |
|-----|------|
| **Magic Access Path** | C++ コード (`ctx.get_field<"...">()`) で EBM フィールドを文字列パスでアクセスするテンプレート API |
| **クエリエンジン文法** | `ebmgen --query` や `--interactive` モードで EBM を検索するコマンドライン専用の文法 |

これらは**別物**です。Magic Access Path はコンパイル時に解決される C++ テンプレート引数であり、クエリエンジン文法は実行時に解釈されるクエリ言語です。

---

## Magic Access Path (C++ API)

`ctx.get_field<"path">()` は EBM フィールドを文字列パスで型安全にアクセスするテンプレート API です。

### アクセス規則

1. **暗黙的な参照解決**: `StatementRef`・`TypeRef`・`ExpressionRef` は対応するインスタンスに自動逆参照される。`IdentifierRef` などは逆参照されない。
2. **名前解決フォールバック**: ルート型 (`Statement`・`Type`・`Expression`) 上にフィールドが見つからない場合、自動的に `body` メンバー内を探す。例: `"kind"` → `body.kind`
3. **Variant/Union 走査**: 上記2つの規則により、variant 型のフィールドにも直接アクセスできる。
4. **`.instance` キーワード**: 参照が指すオブジェクト自体を取得する (メンバーのアクセスではなくオブジェクト取得)。

### Root Type: `Statement`

| アクセスパス | 解決される型 | 説明 |
|------------|------------|------|
| `"id"` | `StatementRef` | ステートメントのユニーク ID |
| `"body"` | `StatementBody` | メイン body union |
| `"kind"` | `StatementKind` | `body.kind` (フォールバック) |
| `"var_decl"` | `VariableDecl` | `body.var_decl` |
| `"var_decl.name"` | `IdentifierRef` | 宣言変数の名前 (逆参照なし) |
| `"var_decl.var_type"` | `TypeRef` | 宣言変数の型参照 |
| `"struct_decl.fields"` | `Block` | struct の全フィールド |
| `"struct_decl.fields.0"` | `StatementRef` | struct の最初のフィールド |
| `"func_decl.return_type"` | `TypeRef` | 関数の戻り値型 |
| `"func_decl.return_type.kind"` | `TypeKind` | 戻り値型の種別 |
| `"if_statement.then_block"` | `StatementRef` | if の then ブロック |
| `"if_statement.then_block.kind"` | `StatementKind` | then ブロック内の種別 |
| `"id.instance"` | `Statement*` | `StatementRef` 逆参照後のオブジェクト |

### Root Type: `Expression`

| アクセスパス | 解決される型 | 説明 |
|------------|------------|------|
| `"id"` | `ExpressionRef` | 式のユニーク ID |
| `"type"` | `TypeRef` | `body.type` (式の結果型) |
| `"type.kind"` | `TypeKind` | 式の結果型の種別 |
| `"kind"` | `ExpressionKind` | `body.kind` |
| `"bop"` | `BinaryOp` | `BINARY_OP` の演算子情報 |
| `"bop.left"` | `ExpressionRef` | 左辺の式 |
| `"bop.left.kind"` | `ExpressionKind` | 左辺の式の種別 |
| `"identifier"` | `StatementRef` | `IDENTIFIER` の宣言へのポインタ |
| `"identifier.kind"` | `StatementKind` | 宣言ステートメントの種別 |
| `"identifier.var_decl.name"` | `IdentifierRef` | 宣言変数の名前 |
| `"call_desc.callee"` | `ExpressionRef` | 関数呼び出しの callee |
| `"call_desc.arguments.0"` | `ExpressionRef` | 最初の引数 |
| `"type.instance"` | `Type*` | `TypeRef` 逆参照後のオブジェクト |

### Root Type: `Type`

| アクセスパス | 解決される型 | 説明 |
|------------|------------|------|
| `"id"` | `TypeRef` | 型のユニーク ID |
| `"body"` | `TypeBody` | メイン body union |
| `"kind"` | `TypeKind` | `body.kind` |
| `"size"` | `Varint` | `INT`/`UINT` のビットサイズ |
| `"element_type"` | `TypeRef` | `ARRAY`/`VECTOR` の要素型 |
| `"element_type.kind"` | `TypeKind` | 要素型の種別 |
| `"body.id"` | `StatementRef` | `STRUCT`/`ENUM` の宣言へのポインタ |
| `"body.id.kind"` | `StatementKind` | 宣言の種別 |
| `"body.id.struct_decl.fields"` | `Block` | struct の全フィールド |
| `"variant_desc.members"` | `Types` | `VARIANT` のメンバー型リスト |
| `"variant_desc.members.0"` | `TypeRef` | `VARIANT` の最初のメンバー型 |
| `"body.id.instance"` | `Statement*` | `StatementRef` 逆参照後のオブジェクト |

EBM 構造の正確な型定義は `src/ebm/extended_binary_module.hpp` を参照してください。

---

## クエリエンジン文法 (コマンドライン専用)

`ebmgen --interactive` または `ebmgen --query` で使用できるクエリ言語です。これは C++ コードとは無関係な、実行時に解釈されるコマンドライン専用の文法です。

```bash
# インタラクティブモード起動
./tool/ebmgen -i input.ebm --interactive

# コマンドラインから直接クエリ実行
./tool/ebmgen -i input.ebm -q "Statement { body.kind == \"IF_STATEMENT\" }"
```

### 構文

```
<ObjectType> { <conditions> }
```

- **ObjectType**: `Identifier`, `String`, `Type`, `Statement`, `Expression`, `Any`
- **条件演算子**: `==`, `!=`, `>`, `>=`, `<`, `<=`, `and`, `or`, `not`, `contains`
- **フィールドアクセス**:
  - `.` でメンバーアクセス
  - `->` で Ref 型を逆参照
  - `[]` でインデックスアクセス

{{< hint info >}}
クエリエンジン文法のフィールドアクセスは Magic Access Path のパス文字列と**似ていますが別物**です。クエリエンジンは実行時に文字列を解析しますが、`ctx.get_field<"...">()` はコンパイル時テンプレートです。
{{< /hint >}}

### 使用例

```bash
# IF_STATEMENT を持つ Statement を検索
./tool/ebmgen -i input.ebm -q "Statement { body.kind == \"IF_STATEMENT\" }"

# STRUCT_DECL をテキスト形式で出力
./tool/ebmgen -i input.ebm -q "Statement { kind == \"STRUCT_DECL\" }" --query-format text

# JSON 形式で出力
./tool/ebmgen -i input.ebm -q "Any {}" --query-format json
```
