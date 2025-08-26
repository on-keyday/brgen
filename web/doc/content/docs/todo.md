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

ToDo リストです(2023/2/16)

- field arguments の意味づけ
  - ジェネレータ側に対応させる
- import 型の対応
  - 多段も許可?
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
  - 重複や、複数回の探索がコストになると思われる。まとめられる部分は一回の探索で置き換えしたい
- パーサーを拡張可能にする
  - パーサーが拡張しづらすぎなので直す
  - LR パーサー的なものを使う? or BNF で書くだけに留める?
- AST 定義を C++から独立させる

  - C++のコードも自動生成する?
  - 他言語でも JSON-AST を生成できるようにする。

- `input = input.subrange`構文の offset は subrange がネストしたときどのように扱えばよいか。
  - subrange は input.offset == 0 のときは subrange における 0 を示す? <-これは微妙だ。これだと全入力の任意の位置は指し示せない。
  - 全体の入力の offset <-こっちが良いと思われる
- Generics の導入?
  - Array とかに対しておんなじ形式?
- WebPlayground を React とかモダンなウェブフレームワーク使うやつに変える
- prefix bit + struct 型にジェネレーターを対応させる
- WebPlayground の行マッピング(C++)を Monaco エディターの仕組みを使ったものに置き換える
  - dirty hack すぎる
- match 文等の網羅性検査

  - enum 型向けのやつ

- キャストの可能性判定(整数型を配列型にキャストしようとしたりを禁止?or 要素化?)
- f :[len]u1(config.type = u32)的な構文

  - ビット列をどうマッピングするかを決定する?。
  - len が 32 ビット超えたらエラー?ビット列長条件は別途?

- input.bit_order, config.bit_order.msb, config.bit_order.lsb の導入?

  - ジェネレーターへの対応

- もっと自動化を推進する
  - C++部分とか
- ビルドスクリプトの整理
  - bat とかいうレガシーシステムの使用はやめる?
- ビットストリームへの対応
- パーサーをエラートレラントにする
- コメントの対応付けをする
  - 現状集めることはできるが、対応付けが曖昧
- AST からコード復元できるようにする

  - ツールで補助?

- input.subrange 構文で offset を指定している場合、input.peek = true と同等の扱いにする
- state 変数に readonly 化する構文をつける?
- available(field,Type) の構文を追加
- union を特定の型にして使うときにはキャストで明示
- テストの強化
  - 型づけのルールをテスト化?
- Diagnostic mode を src2json に実装

  - ジェネレーターから位置とエラーメッセージを受け取って整形して出力?

- config.parent[0]構文

  - 例えば親の親の親は config.parent[2]
  - cast を使って型を明示 `<T>(config.parent[0]).field`

- 適切にヘッダーとソースに分割する

  - 明らかヘッダファイルである必要ないものまでヘッダファイルに書いているので直しましょう

- ブランチベース開発スタイルに変更する

  - main に直 push はやめましょう

- 現在存在する構文の完全なリストの作成
- Field ノードの各フィールドの意味論をはっきりさせる。
  - 作者も混乱してきています...
- Struct Size を 式で表現するようにする
- フォーマット内からフォーマット外へのアクセスをステート変数を除いて制限する
- フィールドにステート変数かどうかを示すやつを追加?
- available(field,Type)構文を if で使ったときに scope 内に元のフィールドへの参照を挿入することで直接解決する
  - TypeScript の型ガード的なもの
- sizeof ビルトイン関数の導入
  - type literal -> 型長さ
  - expr -> 式の型の長さ
- 実際に使ってフィードバックを増やす

- map 型について[<T>]u8 で導入する気になればできそう

  - 型リテラルを index にすれば解決...
  - state 上で使う用に...

- 全体サイズを事前計算できるメソッドは必要そう
  - オーバーヘッドでかいかなと思ったが必要ではありそう
  - ロジック的には encode のをそのまま使えそう

# Done リスト

- float 型(f32 と f64、f80 や f128 なども?)
- 現状はただの引数だが、固定値(整数型)、引数(format 型)、アラインメント指定(共通)、サブレンジ(共通)などの対応
- state 変数の使用の検出と依存関係解決
- AST のドキュメント
  - 自動生成したい
- input.bit_order, config.bit_order.msb, config.bit_order.lsb の導入?
  - gzip とかでのハフマン符号化の格納とか?
- WebPlayground 上で example を触れるようにする
- any match(`..`)が必ず最後にあるかを検証する
- match 文等の網羅性検査?
  - サイズの決定のために
  - 現段階ではサイズが決定可能かつ any match があるときのみサイズを設定?
    - あとは u1 くらい単純なやつ?
- char 型の導入,char リテラル
- import 型の対応
  - i.Type 的なものを。段階は 1 段階のみ
- enum に文字列を割り当てる構文
  - `a = 1, "a"` みたいな
- input.peek = true を指定している場合、フォーマットのサイズ計算から除外する
- input = input.subrange(len,offset)で offset 指定されたときフォーマットのサイズ計算から除外する
