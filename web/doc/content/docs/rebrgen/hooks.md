---
title: "Visitor Hooks"
weight: 3
---

{{< hint info >}}
このドキュメントは AI (Claude) によって生成されたものです。内容に誤りがある場合は [GitHub Issue](https://github.com/on-keyday/brgen/issues) で報告してください。
{{< /hint >}}

# Visitor Hooks

コード生成ロジックはビジターフック (`*_class.hpp`) として実装します。各フックは EBM の特定ノード種別 (例: `Statement_WRITE_DATA`) を処理し、ターゲット言語のコードを文字列として返します。

## フックの解決順序

各フックは生成された `main.cpp` 内の `__has_include` チェーンで7段階に解決されます。`CODEGEN_EXPECTED_PRIORITY_<HOOK>` マクロが最初にマッチした段階の番号 (0–6) を記録します。

| 優先度 | パス | 説明 |
|--------|------|------|
| 0 | `visitor/<Hook>_class.hpp` | 言語固有・クラスベース |
| 1 | `visitor/<Hook>.hpp` | 言語固有・旧形式 (後方互換ラッパーで取り込み) |
| 2 | `visitor/dsl/<Hook>_dsl_class.hpp` | DSL 生成・クラスベース |
| 3 | `visitor/dsl/<Hook>_dsl.hpp` | DSL 生成・旧形式 |
| 4 | `ebmcodegen/default_codegen_visitor/visitor/<Hook>_class.hpp` | デフォルト・クラスベース |
| 5 | `ebmcodegen/default_codegen_visitor/visitor/<Hook>.hpp` | デフォルト・旧形式 |
| 6 | インライン組み込みデフォルト | `visit_unimplemented(...)` を呼び出す (未実装報告) |

**典型的な開発フロー**: 言語ジェネレーターの実装量が少ない段階では、`entry_before_class.hpp` の `std::function` コールバックでデフォルトの挙動を上書きするのが手軽です。コード量が増えてきたタイミングで個別の `<Hook>_class.hpp` (優先度 0) に分割します。

{{< hint info >}}
`default_codegen_visitor/` のカバレッジは現在も拡張中です。対応フックを追加するコントリビューションも歓迎しています。
{{< /hint >}}

## クラスベースフックシステム

### フックの構造

```cpp
#include "../codegen.hpp"

// ファイル先頭の自動生成コメントに ctx の全メンバーが列挙される

DEFINE_VISITOR(Statement_WRITE_DATA) {
    // ctx オブジェクトで EBM フィールドに型安全にアクセス
    auto name = ctx.identifier();

    // 子ノードを visit してコード文字列を取得
    MAYBE(body_res, ctx.visit(ctx.write_data.body));

    CodeWriter w;
    w.writeln("write(", name, ");");
    return w;
}
```

### フックの作成

```bash
# テンプレート生成 (src/ebmcg/ebm2<lang>/visitor/<Hook>_class.hpp を作成)
python script/ebmtemplate.py Statement_WRITE_DATA <lang>

# 利用可能なフック一覧を確認
./tool/ebmcodegen --mode hooklist
```

### `ebmtemplate.py` の主なコマンド

| コマンド | 説明 |
|---------|------|
| `python script/ebmtemplate.py <hook>` | フックのサマリーを標準出力に表示 |
| `python script/ebmtemplate.py <hook> <lang>` | フックファイルを生成 |
| `python script/ebmtemplate.py update <lang>` | 既存フックの自動生成コメントを更新 |
| `python script/ebmtemplate.py list <lang>` | 指定言語の定義済みテンプレート一覧 |
| `python script/ebmtemplate.py interactive` | インタラクティブガイドを起動 |

## ctx オブジェクト

クラスベースフックでは `ctx` オブジェクトを通じてすべての情報にアクセスします。

| アクセス方法 | 内容 |
|-------------|------|
| `ctx.<node_field>` | 訪問中の EBM ノードのフィールド |
| `ctx.identifier()` | 識別子名 (文字列) |
| `ctx.visit(ref)` | 別の EBM ノードを訪問してコードを生成 (`expected<Result>` を返す) |
| `ctx.config()` | `Visitor` 設定オブジェクト |
| `ctx.visitor` | メインビジターオブジェクト |

利用可能なフィールドはフックファイル先頭の自動生成コメントに列挙されます。EBM 構造が変更された場合は `python script/ebmtemplate.py update <lang>` で更新します。

## エラーハンドリング: MAYBE マクロ

```cpp
MAYBE(result, ctx.visit(ctx.some_expr));
// 成功時: result に CodeWriter が入る
// 失敗時: 早期リターン (エラーを伝播)
```

`MAYBE(var, expr)` は Rust の `?` 演算子に相当します。`expected<T>` が失敗値の場合は早期リターンし、成功値は `var` に束縛します。RAII は保持されます。

型定義に不明点があるときは推測せず、必ず `src/ebm/extended_binary_module.hpp` の定義を参照してください。

## デフォルトビジターのカスタマイズ

### `Visitor_before.hpp` — カスタムフィールドの宣言

`src/ebmcg/ebm2<lang>/visitor/Visitor_before.hpp` は `Visitor` 構造体に言語固有のカスタムフィールドを追加します。構造体の宣言ファイルであり、ここには変数宣言を記述します。

```cpp
// 例: Go 言語固有の追加フィールド
std::set<std::string_view> imports;
bool use_io_reader_writer = true;
std::string encode_fn_name;
```

### `entry_before_class.hpp` — 起動時の設定

`src/ebmcg/ebm2<lang>/visitor/entry_before_class.hpp` は `DEFINE_VISITOR(entry_before)` フックとして実装し、コード生成開始時に `ctx.config()` の設定値や `std::function` コールバックをセットします。

```cpp
DEFINE_VISITOR(entry_before) {
    ctx.config().function_define_keyword = "func";
    ctx.config().begin_block = " {";

    ctx.config().enum_decl_visitor = [&](Context_Statement_ENUM_DECL& ectx) -> expected<Result> {
        // ...
        return w;
    };

    return pass;
}
```

{{< hint info >}}
一部の既存ジェネレーター (Python など) では `entry_before.hpp` という旧形式のファイルが残っています。これはマイグレーションコストの都合で `ebmcodegen/class_based.cpp` 内で互換維持されているもので、新規実装では使用しないでください。既存の旧形式は順次クラスベース形式 (`entry_before_class.hpp`) への切り替えが推奨されます。
{{< /hint >}}

### `std::function` コールバックの命名規則

`default_codegen_visitor/Visitor.hpp` に定義されているコールバックの命名規則:

| サフィックス | 戻り値の契約 |
|------------|------------|
| `*_custom` | 処理した場合は `expected<CodeWriter>` を返す。未処理の場合は `pass` を返してデフォルト処理に委ねる |
| `*_visitor` | 必ず `expected<CodeWriter>` を返す (完全オーバーライド) |
| `*_wrapper` | デフォルト処理の前後フック。戻り値の契約は `*_visitor` と同じ |

{{< hint warning >}}
この命名規則は現時点では安定していない場合があります。
{{< /hint >}}

## DSL (実験的)

`ebmcodegen` は訪問フックを記述するための DSL をサポートしています。`src/ebmcg/ebm2python/dsl_sample/` に実例があります。

```
{% C++ コード %}        - C++ リテラルコードを埋め込む
{{ C++ 式 }}           - 式の結果を出力に書き込む
{* EBM ノード式 *}      - EBM ノードを処理し出力に書き込む
{& 識別子式 &}          - 識別子を取得し出力に書き込む
{! for/if/endif 等 !}  - 制御フロー
```

DSL ファイルを C++ フックにコンパイルするには `python script/ebmdsl.py` を使用します。
