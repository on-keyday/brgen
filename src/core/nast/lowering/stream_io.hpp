/*license*/
#pragma once
#include "lowering.hpp"

// 入出力からバイトを出し入れする形。言語が持っている語彙をそのまま使う:
//
//   input.get()     1 バイト読む (引数なしの既定は u8)
//   output.put(x)   1 バイト書く
//
// 手書きの `fn decode` / `fn encode` が使っているものと同じで、コーパスに
// 74 箇所ある。ADR 0045 が「backend が用意すべき IO ランタイムはバイト列の
// read/write プリミティブ」と言っているものに当たるが、**新しく作る必要は
// なく既に言語にある**。
//
// **文にして並べる。** `(u16(input.get()) << 8) | u16(input.get())` のように
// 式の中へ直接置くと、評価順の保証が言語ごとに違うので上位と下位が入れ替わり
// うる。ebm2go の生成物が一時配列に読んでから合成しているのも同じ理由。
// ここが int_bytes と分かれているのはそのためで、こちらが「バイトを並べる」、
// あちらが「並んだバイトから値を組む」を受け持つ。
//
// 位置は進めない。`input.get()` 自身が進むので、こちらで数える必要が無い。
// 入れ物 (一時配列) の宣言も組まない — 呼ぶ側の領分
// (lowering/conditional が宣言を合成しないのと同じ線引き)。

namespace brgen::nast::lowering {

    // 既定の入出力。`input` / `output` そのもの。subrange など別のストリーム値
    // を使うときは呼ぶ側がその式を渡す。
    Node<Expr> input_stream(Context& c, lexer::Loc loc);
    Node<Expr> output_stream(Context& c, lexer::Loc loc);

    // stream から count バイト読んで buffer[offset..] に入れる形。
    // count が定数なら並べ、そうでなければ回す。
    Node<Body> read_bytes(Context& c, Node<Expr> stream, Node<Expr> buffer, Node<Expr> offset,
                          Node<Expr> count);

    // buffer[offset..] の count バイトを stream へ書く形。
    Node<Body> write_bytes(Context& c, Node<Expr> stream, Node<Expr> buffer, Node<Expr> offset,
                           Node<Expr> count);

}  // namespace brgen::nast::lowering
