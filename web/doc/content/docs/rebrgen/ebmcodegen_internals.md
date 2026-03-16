---
title: "ebmcodegen 内部アーキテクチャ"
weight: 8
---

{{< hint info >}}
このドキュメントは AI (Claude) によってソースコードを解析して生成されたものです。内容に誤りがある場合は [GitHub Issue](https://github.com/on-keyday/brgen/issues) で報告してください。
{{< /hint >}}

# ebmcodegen 内部アーキテクチャ

`ebmcodegen` は EBM の型構造を入力として、言語ジェネレーター (`ebm2<lang>`) の C++ コードを生成するメタジェネレーターです。このページでは `src/ebmcodegen/` の内部実装を説明します。

## 全体の役割

新しい言語ジェネレーターを作る際に必要な定型コードのほとんどを自動生成します:

- ビジターのディスパッチ基盤 (concepts、dispatch 関数、context クラス)
- フックのインクルードシステム (多段フォールバック、クラスベース生成)
- 型付きコンテキスト (クラスベースフックでの IDE オートコンプリート用)
- CLI スキャフォールディング (`Flags`、`Output`、エントリーポイント)
- EBM 構造のリフレクション (フィールド可用性、JSON 変換、アクセス API)

## ソースファイル構成

```
src/ebmcodegen/
  main.cpp          モード分岐・エントリーポイント (1333行)
  class_based.cpp   クラスベースフック生成の核心 (2099行)
  structs.cpp       EBM 構造リフレクション (163行)
  body_subset.cpp   各 Kind のフィールド可用性テーブル [自動生成]
  access.cpp        constexpr フィールドアクセス API 生成 (379行)
  json_loader.cpp   JSON デシリアライズコード生成 (91行)
  stub/             基盤インフラ (ヘッダー群)
  dsl/              DSL コンパイラー
  default_codegen_visitor/  デフォルトフォールバック実装
  default_interpret_visitor/ インタープリター用デフォルト実装
```

---

## 各ファイルの責務

### `main.cpp` — モード分岐・エントリーポイント

全生成モードのルーティングを担当します。`--mode` フラグに応じて適切な生成関数を呼び出します。

**主な生成関数:**

| 関数 | 出力 |
|------|------|
| `print_cmake()` | `CMakeLists.txt` |
| `print_spec_json()` | EBM 構造の JSON 仕様 |
| `print_body_subset()` | `body_subset.cpp` (各 Kind のフィールド可用性) |
| `print_json_converter()` | JSON シリアライズ/デシリアライズコード |
| `print_hook_kind()` | 全フック名と suffix の JSON |

**フック名のパース:** `parse_hook_name()` が `Expression_BINARY_OP_class.hpp` のようなファイル名を解析し、対象 (Expression)、種別 (BINARY_OP)、位置 suffix (_before/_after/_class など) を抽出します。

**フックの prefix/suffix 体系:**

- Prefix: `entry`, `includes`, `pre_visitor`, `post_entry`, `Visitor`, `Flags`, `Output`, `Result`, `Statement`, `Expression`, `Type` など
- Suffix: `_before`, `_after`, `_pre_default`, `_post_default`, `_class`, `_dsl` など

---

### `class_based.cpp` — クラスベースフック生成 (最大ファイル)

現代のクラスベースフックシステムを実装する核心部です。

#### ContextClass

各ビジターフックのコンテキストを表す構造体:

```cpp
struct ContextClass {
    std::string_view base;     // Statement, Expression, Type, entry, ...
    std::string_view variant;  // BLOCK, ASSIGNMENT, BINARY_OP, ...
    ContextClassKind kind;     // Normal, Before, After, List, Generic, Special, Observe
    std::vector<TypeParam> type_params;
    std::vector<ContextClassField> fields;  // visitor, item_id, フィールド参照 etc.
};
```

`ContextClassKind` はビットフラグで以下を組み合わせます:

| フラグ | 意味 |
|--------|------|
| `Normal` (0) | 通常のバリアントフック (例: `Statement_BLOCK_class.hpp`) |
| `Before` (1) | Before フック (観察・早期リターンによる乗っ取り可能) |
| `After` (2) | After フック |
| `List` (4) | コンテナバリアント (Statements, Expressions, Types) |
| `Generic` (8) | 種別による dispatch (Statement 全体など) |
| `Special` (16) | entry, pre_visitor, post_entry |
| `Observe` (32) | 読み取り専用の観察フック |

#### 生成パイプライン

`generate_context_classes()` が以下を生成します:

1. Special コンテキスト (entry, pre_visitor, post_entry)
2. Statement/Expression/Type × 全 Kind ごとのコンテキスト (body_subset から取得)
3. Generic コンテキスト (種別による dispatch 用)
4. List コンテキスト (Block/Statements/Expressions/Types の反復用)

各コンテキストに対して:
- `generate_visitor_requirements()`: C++ concepts でビジター型チェック
- `generate_context_class()`: 型付きコンテキストクラス定義の出力
- `generate_user_implemented_includes()`: ユーザーフックのインクルード (フォールバック付き)

#### フックのフォールバックチェーン

生成されるコードは `__has_include` による多段フォールバックを使います:

```cpp
// 1. 言語固有オーバーライド
#if __has_include("visitor/Statement_BLOCK_class.hpp")
#include "visitor/Statement_BLOCK_class.hpp"
// 2. DSL 生成オーバーライド
#elif __has_include("visitor/dsl/Statement_BLOCK_dsl.hpp")
#include "visitor/dsl/Statement_BLOCK_dsl.hpp"
// 3. デフォルトフォールバック
#elif __has_include("default/Statement_BLOCK_class.hpp")
#include "default/Statement_BLOCK_class.hpp"
#endif
```

#### 旧形式との互換 (`entry_before.hpp`)

`generate_inlined_hook()` が旧形式 (マクロなし、直接コード記述) のフックファイルを現代のシステムにラップして互換性を保ちます。`entry_before.hpp` がこの仕組みで温存されています。

---

### `structs.cpp` — EBM 構造リフレクション

C++20 の constexpr ビジターパターンで `ebm::ExtendedBinaryModule` を走査し、全構造体フィールドと列挙型メンバーを抽出します。

`make_struct_map()` が返すマップ (構造体名 → Struct, 列挙型名 → Enum) が `class_based.cpp` での Kind ごとのボディクラス生成に使われます。

---

### `body_subset.cpp` — フィールド可用性テーブル [自動生成]

`ebmcodegen --mode subset` が生成する自動生成ファイル。各 Statement/Expression/Type の Kind ごとに、その Kind で実際に値が入っているフィールド名を返す関数群を定義します。

```cpp
// 生成されるコードのイメージ
std::map<ebm::StatementKind, std::pair<std::set<std::string_view>, std::vector<std::string_view>>>
body_subset_StatementBody() {
    // StatementKind::BLOCK → {"block", "fields", ...}
    // StatementKind::IF_STATEMENT → {"condition", "then_block", "else_block", ...}
    // ...
}
```

`class_based.cpp` がこれを使ってコンテキストクラスのフィールドを決定するため、**このファイルを手動で編集してはならない。**

---

### `access.cpp` — Magic Access Path API 生成

コンパイル時フィールドアクセス基盤を生成します。`ctx.get_field<"field.path">()` の実装がここから来ます。

**生成されるパターン:**

```cpp
template<> constexpr size_t get_type_index<ebm::Statement>() { return 5; }
template<> constexpr size_t get_field_index<5>(std::string_view name) {
    if (name == "id") return 0;
    if (name == "body") return 1;
    // ...
}
template<size_t I> constexpr decltype(auto) get_field(auto&& in);
// 各 I への特殊化
```

これにより `ctx.get_field<"struct_decl.fields.0">()` のようなドット区切りパスがコンパイル時に解決されます。

---

### `json_loader.cpp` — JSON デシリアライズコード生成

各構造体/列挙型に対して `from_json()` 関数のボイラープレートを生成します。EBM JSON を C++ 構造体に変換するために使われます。

---

## `stub/` — 基盤インフラ

| ファイル | 内容 |
|---------|------|
| `hooks.hpp` | フック命名システム (prefix/suffix 配列、ParsedHookName 構造体) |
| `structs.hpp` | データモデル (StructField, Struct, Enum, TypeAttribute) |
| `class_based.hpp` | クラスベース生成 API の宣言 (IncludeLocations, generate()) |
| `visitor.hpp` | `DEFINE_VISITOR` マクロ定義 |
| `context.hpp`, `entry.hpp`, `util.hpp`, `output.hpp` | コード生成ランタイムのサポート |
| `code_writer.hpp`, `writer_manager.hpp` | コード出力バッファー |

---

## DSL サブシステム (`dsl/`)

`.dsl` ファイルを C++ フック実装 (`visitor/dsl/*.hpp`) に変換するコンパイラーです。

**構文:**

| マーカー | 意味 |
|---------|------|
| `{% ... %}` | C++ リテラルコードを埋め込む |
| `{{ expr }}` | 式の結果を出力に書き込む |
| `{* node *}` | EBM ノードを visit して出力に書き込む |
| `{& id &}` | 識別子を取得して出力に書き込む |
| `{! for/if/endif/... !}` | 制御フロー |

DSL の実例は `src/ebmcg/ebm2python/dsl_sample/` にあります。

---

## `default_codegen_visitor/` と `default_interpret_visitor/`

言語固有フックが存在しない場合のデフォルト実装を提供する**手書き**のフック群です。`ebmcodegen` が生成するファイルではありません。

- `default_codegen_visitor/`: コードジェネレーター用デフォルト (テキスト生成)
- `default_interpret_visitor/`: インタープリター用デフォルト (`Instruction` オブジェクト生成)

`Visitor` 構造体の設定フィールドや `std::function` コールバックを通じて、多くの言語固有挙動を言語ジェネレーター側から制御できるようになっています。

---

## 新しい言語ジェネレーター作成の流れ (まとめ)

1. `python script/ebmcodegen.py <lang>` — スケルトン生成 (`main.cpp`, `CMakeLists.txt`)
2. `python script/build.py` — ビルド
3. `python script/unictest.py --target-runner ebm2<lang>` — 未実装フックを特定
4. `python script/ebmtemplate.py <Hook> <lang>` — フックファイルを生成
5. `entry_before_class.hpp` で `ctx.config()` を設定 (基本構文トークン、`std::function` コールバック)
6. `Visitor_before.hpp` で言語固有のカスタムフィールドを追加
7. 個別フック (`*_class.hpp`) でノード種別ごとのコード生成ロジックを実装
8. テストを繰り返す
