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

ToDo リストです(2023/1/15)

- field arguments の意味づけ
  - ジェネレータ側に対応させる
- import 型の対応
  - i.Type 的なものを。段階は 1 段階のみ?多段も許可?
- union 型のメンバー
  - union 型でも a.b とできるようにする
- 可変ビット整数型
  - swf_rect.bgn など
- custom format の実装
  - encode と decode あとオプションで size?
- src/core/ast/json.h のリファクタリング
  - `expected<T,E>` とそれを簡単にする演算子を使おうとした結果、作者ですら意味不明なものができてしまっているのでリファクタリングしたい
- float 型対応
  - バイナリレベルキャスト?
- src/middle 内の AST 置き換え処理をまとめる
  - 重複や、複数回の探索がコストになると思われる。一回の探索で置き換えしたい
- パーサーを拡張可能にする
  - パーサーが拡張しづらすぎなので直す
- AST 定義を C++から独立させる
  - C++のコードも自動生成する?
  - 他言語でも JSON-AST を生成できるようにする
- enum に文字列を割り当てる構文
  - `a = 1, "a"` みたいな
- AST のドキュメント
  - 自動生成したい
- `input = input.subrange`構文の offset は subrange がネストしたときどのように扱えばよいか。
  - subrange は input.offset == 0 のときは subrange における 0 を示す? <-これは微妙だ。これだと全入力の任意の位置は指し示せない。
  - 全体の入力の offset <-こっちが良いと思われる
- Generics の導入?
  - Array とかに対しておんなじ形式?
- WebPlayground を React とかモダンなウェブフレームワーク使うやつに変える
- prefix bit + struct 型にジェネレーターを対応させる
- WebPlayground の行マッピング(C++)を Monaco エディターの仕組みを使ったものに置き換える
  - dirty hack すぎる

# Done リスト

- float 型(f32 と f64、f80 や f128 なども?)
- 現状はただの引数だが、固定値(整数型)、引数(format 型)、アラインメント指定(共通)、サブレンジ(共通)などの対応
