/*license*/
#pragma once
#include "lowering.hpp"

// field 1 つを読む / 書く形。int_bytes の外側で、型に応じて組み立てる。
//
//   整数     int_bytes をそのまま
//   enum     下地の整数として読み書きし、値を enum に落とす / 下地へ上げる
//            (EBM の ENUM_UNDERLYING_TO_INT)
//   配列     要素を回す。要素の読み書きは同じ規則を再帰で使う
//            (EBM の ARRAY_FOR_EACH)
//
// **offset を進めるところは組まない。** 何バイト読んだかをどう持つかは IO の
// 表し方と密に結びついていて、IR で一意にできないと EBM が一度撤回している
// (ADR 0008)。ここは「この位置から読む」形までを返し、位置の管理は呼ぶ側。
// 配列の要素の位置は要素幅が固定のときだけ組める (`offset + i * 幅`) ので、
// 要素が可変幅の配列は組めない — 進みながら読む形になり、上と同じ理由で
// 呼ぶ側の領分になる。
//
// バッファと位置は呼ぶ側が用意する。int_bytes と同じ線引き。
//
// **分岐が合成した同名 field (型が UnionType) は対象ではない。** あれは名前
// 解決のための人工物で、実際に読むのは分岐の中に並んでいる field のほう
// (`node/util.h` の `is_layout_field`)。渡されても組まない。
//
// **field の引数** (`node/util.h` の `field_args`) は 2 通りに分かれる。
//
// 位置で書かれた `f :T(期待値)` は「その位置を読んだ値がこれと等しいこと」で、
// 読む側は読んだ後に、書く側は書く前に assert を置く。書いてから検査しても
// 失敗した時点でバッファは汚れているし、期待値を黙って代入して直す形にすると
// 呼ぶ側が入れた値を上書きする。配列の field に付いたときは**要素と比べる** —
// `[20]u8(0)` は 20 要素すべてが 0 の意味 (原実装が repeat mapping と呼ぶ)。
// 期待値そのものが配列なら全体と比べる。書かれ方では決まらないので期待値の型
// で分ける。位置指定が 2 つ以上ある `u8(7,128..255)` は「どれかに一致」で、
// 比較 1 つには落ちないので組めない。
//
// 名前つきの引数は 5 種類 (`input =` / `input.align` / `input.peek` /
// `config.type` / 呼ぶ先の state への代入)。どれもバッファと位置そのものを
// 差し替えるので上の線引きの外にあり、付いていたら組めない扱いにする。
// 内訳は docs/size_and_lowering.md にある。

namespace brgen::nast::lowering {

    // 組めなければ null。今のところ組めないもの:
    //   要素が可変幅の配列 / 末尾までの配列 / 入れ子 format /
    //   バイト境界に乗らない幅 / 名前つきの引数が付いた field
    Node<Body> lower_field_decode(Context& c, Node<Field> f, Node<Expr> target, Node<Expr> bytes,
                                  Node<Expr> offset);

    Node<Body> lower_field_encode(Context& c, Node<Field> f, Node<Expr> target, Node<Expr> bytes,
                                  Node<Expr> offset);

}  // namespace brgen::nast::lowering
