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

get_size_str: Size構造体を受け取ってサイズを表す文字列の入ったCodeWriterを返す。
