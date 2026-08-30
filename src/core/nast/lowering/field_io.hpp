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
// 要素が可変幅の配列は断る — 進みながら読む形になり、上と同じ理由で
// 呼ぶ側の領分になる。
//
// バッファと位置は呼ぶ側が用意する。int_bytes と同じ線引き。
//
// **分岐が合成した同名 field (型が UnionType) は対象ではない。** あれは名前
// 解決のための人工物で、実際に読むのは分岐の中に並んでいる field のほう
// (`node/util.h` の `is_layout_field`)。渡されたら断る。

namespace brgen::nast::lowering {

    // 組めなければ null。今のところ組めないもの:
    //   要素が可変幅の配列 / 末尾までの配列 / 入れ子 format /
    //   バイト境界に乗らない幅
    Node<Body> lower_field_decode(Context& c, Node<Field> f, Node<Expr> target, Node<Expr> bytes,
                                  Node<Expr> offset);

    Node<Body> lower_field_encode(Context& c, Node<Field> f, Node<Expr> target, Node<Expr> bytes,
                                  Node<Expr> offset);

}  // namespace brgen::nast::lowering
