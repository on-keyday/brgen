---
title: "Todo"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# ToDo

ToDo リストです(2023/1/7)

- field arguments の意味づけ
  - 現状はただの引数だが、固定値(整数型)、引数(format 型)、アラインメント指定(共通)、サブレンジ(共通)などの対応
- import 型の対応
  - i.Type 的なものを。段階は 1 段階のみ?多段も許可?
- 可変ビット整数型
  - swf_rect.bgn など
- custom format の実装
  - encode と decode あとオプションで size?
- src/core/ast/json.h のリファクタリング
  - `expected<T,E>` とそれを簡単にする演算子を使おうとした結果、作者ですら意味不明なものができてしまっているのでリファクタリングしたい
- float 型対応
  - float 型(f32 と f64、f80 や f128 なども?)、バイナリレベルキャスト?
- src/middle 内の AST 置き換え処理をまとめる
  - 重複や、複数回の探索がコストになると思われる。一回の探索で置き換えしたい
- パーサーを拡張可能にする
  - パーサーが拡張しづらすぎなので直す
- AST 定義を C++から独立させる
  - C++のコードも自動生成する?
  - 他言語でも JSON-AST を生成できるようにする
