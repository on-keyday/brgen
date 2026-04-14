# ctx.get_field<"field_decl">() API の導入

## 日付

- 判断時期: 2025年11月28日頃（class-based移行直後に accessor として導入）
- 文書化: 2026-04-15

## 判断

EBM の AST の深い階層から情報を取得する際の null チェックの連鎖を隠蔽する
テンプレート API（ctx.get_field<"field_decl">() 等）を導入した。

## 動機

- **easy to write and read**: EBM のノードを辿って情報を取得するには
  参照の解決 → null チェック → body の種別判定 → フィールドアクセス
  という手順を毎回書く必要があった。
- null チェック忘れが起きやすい。実際 AI に書かせると null チェックを飛ばすことがあった。
- 人間が書いても if のネストが深くなり読みづらい。

## 具体例

```cpp
// Before: 手動で辿る
if (auto x = ctx.get(ref)) {
    if (auto field_decl = x->body.field_decl()) {
        // ようやく使える
    }
}

// After: 一発で取れる
auto field = ctx.get_field<"field_decl">(ref);

// MAYBE と組み合わせて、確実にあるはずの場合はエラー伝搬付きで取る
MAYBE(field, ctx.get_field<"field_decl">(ref));
```

## これは X を意味しない

- すべての EBM アクセスをこの API 経由にするという方針ではない。
  深い階層を辿るパターンが頻出する箇所向け。
