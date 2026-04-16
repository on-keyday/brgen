---
name: rebrgen-dev-tips
description: 開発上重要なtips。例えばどの機能がどこに定義されていてどんな構造になっているかの概要です。マクロやContext_XXXなどについてわかります。開発中に「どこに何があるんだっけ or let me know...」となったらここを参照すること。
---

## マクロ

### MAYBE/MAYBE_VOID

MAYBE/MAYBE_VOIDはエラーハンドリング用マクロで実体はsrc/ebmgen/common.hppにある。
おおよそ内容としてはこういう感じで実際はこれを識別子衝突の回避やexprがポインタ型の場合等の場合の考慮など拡張したやつが定義されている。MAYBE_VOIDはexpected<void>用。

```cpp
#define MAYBE_VOID(ident,expr)
    auto ident##__ = (expr); \
    if (!_result) return unexpected(_result.error());

#define MAYBE(ident, expr) \
    MAYBE_VOID(ident, expr) \
    auto ident = *ident##__;
```

### DEFINE_VISITOR

DEFINE_VISITORはコードジェネレーターのビジターフレームワーク用のマクロでsrc/ebmcodegen/stub/visitor.hppにある。実体はテンプレートクラスの特殊化でおおよそ以下のような感じ。

```cpp
#define DEFINE_VISITOR(name) \
  template<>   \
  struct Visitor<Tag_##name> { \
    expected<Result> visit(Context_##name& ctx); \
  } \
  expected<Result> Visitor<Tag_##name>::visit(Context_##name& ctx)
```

## Context_XXX

Context_XXXはコードジェネレーターのビジターフレームワーク用の構造体でsrc/ebmcodegen/stub/visitor/context.hppに基底クラスとなるContextBaseクラステンプレートが定義されており各ebm2<lang>/codegen.hppに実体が定義されている。例えば以下のような感じ。具体的にはsrc/ebmcodegen/class_based.cppのコード生成ロジックで生成されているが生成ロジックを見たところであんまり理解の助けにはならないので素直にcodegen.hppの定義を見たほうがいいと思う。

```cpp
struct Context_Expression_HOGEHOGE : ebmcodegen::util::ContextBase<Context_Expression_HOGEHOGE> {
    BaseVisitor& visitor;
    const ebm::ExpressionRef& other_expr;
};
```

## util

src/ebmcodegen/stub/util.hppに定義されている便利関数群。例えば以下のようなものがある。

- get_size_str: Size構造体を受け取ってサイズを表す文字列の入ったCodeWriterを返す。
- emit_struct_methods: 構造体のメソッド群（properties, encode/decode, methods）をCodeWriterに書き出す共通ロジック。デフォルトSTRUCT_DECLから切り出されたもの。
- sorted_struct (dependency.hpp): 構造体の依存関係をトポロジカルソートして返す。前方参照解決に使う。

---

## codegen.hpp と main.cpp の構造

各`ebm2<lang>/`には2つの自動生成ファイルがある。どちらもebmcodegenが生成するので手動編集禁止。

### codegen.hpp（型定義）

namespace ebm2<lang> 内に以下が定義される:

```
namespace ebm2<lang> {
    struct MergedVisitor;  // 前方宣言のみ（実体はmain.cpp）
    struct Result { ... };
    struct Flags : ebmcodegen::Flags { ... };  // コマンドラインフラグ
    struct Output : ebmcodegen::Output { ... };

    // ★ 全 Context_XXX の定義（数百個）
    // 例: Context_Statement_STRUCT_DECL, Context_Expression_BINARY_OP, ...
    // 各Contextにはvisitor, item_id, そしてノード固有のフィールドがメンバとして入る

    // ★ 各Contextに対応する VisitorTag_XXX（空struct、テンプレートのタグ）

    // ★ BaseVisitor 構造体
    struct BaseVisitor {
        static constexpr const char* program_name = "ebm2<lang>";
        Flags& flags;
        Output& output;
        WriterManager<CodeWriter> wm;
        MappingTable module_;

        #include "visitor/Visitor_before.hpp"  // 言語固有の追加フィールド
        #include "visitor/Visitor.hpp"         // config（std::functionフック群）
        #include "visitor/Visitor_after.hpp"
    };
}
```

`ctx.config()` は `BaseVisitor&` を返す。
つまり Visitor.hpp のフック群も Visitor_before.hpp のフィールドも全て BaseVisitor のメンバ。

### main.cpp（visitor実装 + ディスパッチ）

```
namespace ebm2<lang> {
    // ★ 各 *_class.hpp をincludeしてVisitor特殊化を定義
    //   entry_before_class.hpp → Visitor<UserHook<VisitorTag_entry_before>>
    //   Statement_STRUCT_DECL_class.hpp → Visitor<UserHook<VisitorTag_Statement_STRUCT_DECL>>
    //   ...（全EBMノード種別 × {before, main, after} の組み合わせ）

    // ★ VisitorsImpl: 全Visitor特殊化をまとめたディスパッチテーブル
    // ★ MergedVisitor : BaseVisitor
    //   visit(Context_XXX&) → VisitorsImpl経由で対応するVisitor::visit()を呼ぶ
    //   つまり ctx.visit(ref) → MergedVisitor::visit() → Visitor<Tag>::visit(ctx)
}
```

### visitor/ ディレクトリのファイルとinclude先の対応

| ファイル | include先 | 役割 |
|---------|----------|------|
| `Visitor_before.hpp` | codegen.hpp内 BaseVisitor構造体のメンバ | 言語固有フィールド追加（enum, bool等） |
| `Visitor.hpp` | codegen.hpp内 BaseVisitor構造体のメンバ | configフック定義（通常はdefault使用） |
| `Flags.hpp` | codegen.hpp内 Flags構造体のメンバ | コマンドラインフラグ定義 |
| `entry_before_class.hpp` | main.cpp | configフック設定（ここが言語実装の主要ファイル） |
| `Statement_XXX_class.hpp` | main.cpp | 個別ノードのvisitor実装 |
| `includes.hpp` | codegen.hpp冒頭（namespace外） | 追加#include |

### include探索の優先順位

全てのvisitor includeポイントで3段階フォールバック:
1. `visitor/<Name>.hpp` — 言語固有（最優先）
2. `visitor/dsl/<Name>_dsl.hpp` — DSL生成版
3. `ebmcodegen/default_codegen_visitor/visitor/<Name>.hpp` — デフォルト

ファイルが存在しなければ次の段階へ。つまり言語固有ファイルを置けばデフォルトを上書きできる。

### スコープの注意点

Visitor_before.hppに書いた定義はBaseVisitorのメンバになる。
つまり `enum class Foo` を書くと `BaseVisitor::Foo` になり、ラムダからは直接 `Foo` と書けない。
ラムダで使うには:

```cpp
// entry_before_class.hpp 内のDEFINE_VISITOR内
using Foo = std::remove_reference_t<decltype(ctx.config())>::Foo;
// これ以降、ラムダ内で Foo が使える
```

---

## フック実行フローと優先度

デフォルトvisitorにおけるフック呼び出しの一般パターン:

### 1. `*_custom` フック（CALL_OR_PASS）

```cpp
if (ctx.config().foo_custom) {
    CALL_OR_PASS(result, ctx.config().foo_custom(ctx));
}
// custom が pass を返した → ここに来る（デフォルト処理に進む）
// custom が Result を返した → この関数全体からそのResultが返る
// custom がエラーを返した → エラーが伝播する
```

**`pass`を返す = 「自分は処理しない、デフォルトに任せる」**。
Resultを返す = デフォルト処理を完全にスキップしてその結果を使う。
条件分岐で一部だけカスタムしたい場合は、条件に合わない場合に`pass`を返す。

### 2. `*_wrapper` フック（部分カスタマイズ）

```cpp
if (ctx.config().foo_start_wrapper) {
    MAYBE(start, ctx.config().foo_start_wrapper(ctx));
    w.write(start.to_writer());
} else {
    // デフォルトの開始部分
}
// ... デフォルトの中身 ...
if (ctx.config().foo_end_wrapper) {
    return ctx.config().foo_end_wrapper(ctx, w);
}
```

wrapperは呼ばれたら**必ず結果を使う**（passの概念がない）。
構造の一部（開始/終了）だけ変えたい場合に使う。

### 3. `*_visitor` フック（旧式、非推奨予定）

```cpp
if (ctx.config().foo_visitor) {
    return ctx.config().foo_visitor(ctx);  // 完全に乗っ取る
}
```

**注意: `*_visitor`は`*_custom`の下位互換であり、今後`*_custom`に統一予定。**
`*_visitor`は設定されると無条件に乗っ取る（pass不可）。
`*_custom`はpassで「やっぱりデフォルトで」ができるため柔軟。
新規コードでは`*_custom`を使うこと。既存の`*_visitor`も順次`*_custom`にリネーム予定。

### 具体例: STRUCT_DECLの実行フロー

```
DEFINE_VISITOR(Statement_STRUCT_DECL):
  1. struct_decl_custom → CALL_OR_PASS（passならデフォルトへ）
  2. struct_definition_start_wrapper → 構造体定義の開始行
  3. ctx.visit(fields) → フィールド定義
  4. methods_inner_class == true → emit_struct_methods() → メソッド
  5. "}" + endof_struct_definition
  6. methods_inner_class == false → emit_struct_methods() → メソッド
  7. struct_definition_end_wrapper → 最終加工
```

### 具体例: FUNCTION_DECLの実行フロー

```
DEFINE_VISITOR(Statement_FUNCTION_DECL):
  1. function_decl_custom → CALL_OR_PASS（passならデフォルトへ）
  2. params組み立て
  3. function_definition_start_wrapper → シグネチャ+開き括弧
  4. ctx.visit(body) → 関数本体
  5. "}"
  6. wrapper_function があれば再帰visit
```

### 具体例: PROGRAM_DECLの実行フロー

```
DEFINE_VISITOR(Statement_PROGRAM_DECL):
  1. program_decl_custom → CALL_OR_PASS（passならデフォルトへ）
  2. program_decl_start_wrapper → ヘッダ（#pragma once等）
  3. for each stmt in block.container → ctx.visit(stmt) + decl_toplevel処理
  4. program_decl_end_wrapper → 最終加工
```

---

## フェーズパターン（複数回visitで出力を変える）

前方参照の解決や宣言/定義の分離が必要な言語（C, C++等）では、
configにフェーズフラグを持たせて同じvisitorを複数回呼ぶパターンが使われる。

### ebm2cの例: `forward_decl`フラグ

```
// Visitor_before.hpp
bool forward_decl = false;
```

PROGRAM_DECLのカスタム処理内で:
```
Phase 1: forward_decl = true  → foreach_function() → シグネチャのみ（";"で終わる）
Phase 2: forward_decl = false → foreach_function() → 完全な関数定義
```

FUNCTION_DECL内部で分岐:
```cpp
if (ctx.config().forward_decl) {
    w.writeln(ret_type, " ", name, "(", params, ");");  // シグネチャだけ
    return w;
}
w.writeln(ret_type, " ", name, "(", params, ") {");
// ... body ...
```

### ebm2cppの例: `OutputPhase` enum

```
// Visitor_before.hpp
enum class OutputPhase { Normal, DeclarationOnly, FunctionBodyOnly };
OutputPhase output_phase = OutputPhase::Normal;
```

PROGRAM_DECLのprogram_decl_custom内で3パス:
```
Phase 1: enum出力 + sorted_structで前方宣言
Phase 2: output_phase = DeclarationOnly → struct定義 + メソッドシグネチャ
Phase 3: output_phase = FunctionBodyOnly → out-of-classメソッド定義
```

function_decl_customとstruct_decl_customで分岐:
- DeclarationOnly → シグネチャ+";", struct定義はデフォルトパス
- FunctionBodyOnly → "RetType StructName::method(...) { body }", struct定義はスキップ

### パターンの要点

1. Visitor_before.hppにフェーズ制御用のフラグ/enumを追加
2. program_decl_customで全体を制御（sorted_structで順序保証）
3. 各ノードの`*_custom`フックでフェーズに応じて出力を切り替え
4. `pass`を返せばデフォルト処理にフォールバック可能

---

## sorted_struct（前方参照解決）

`ebmcodegen/stub/dependency.hpp`に定義。構造体の依存関係を解析しトポロジカルソートする。

```cpp
MAYBE(sorted, sorted_struct(ctx));  // ctx は PROGRAM_DECL等のcontext
// sorted: vector<StatementRef> — 依存先が先に来る順序
```

依存として追跡するもの:
- フィールドの型がSTRUCT → そのstructに依存
- フィールドの型がSTRUCT_UNION → 各メンバstructに依存
- フィールドの型がVARIANT → 各メンバに依存

循環参照（RECURSIVE_STRUCT）は追跡しない（ポインタで解決されるため）。
