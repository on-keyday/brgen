/*license*/
#pragma once
#include "lowering.hpp"

// 整数とバイト列の相互変換。EBM の INT_TO_BYTE_ARRAY / ARRAY_TO_INT に当たる。
//
//   combine_int   バイト列 -> 値   (u16(b[0]) << 8) | u16(b[1])
//   split_int     値 -> バイト列   b[0] = u8(v >> 8)
//                                  b[1] = u8(v)
//
// ADR 0045 が「backend が用意すべき IO ランタイムはバイト列の read/write
// プリミティブだけ」と言い切っている根拠がここで、エンディアンと合成は
// 変換側が一度書けば済む。実際 ebm2java は scalar 用の経路を全部消して
// readBytes/writeBytes だけにした。これを各バックエンドで書き直すと、
// エンディアンの取り違えを言語の数だけ作ることになる。
//
// **これは他の規則と違い side table に載せない。** 出力が「呼ぶ側が用意した
// バッファの名前」に依存するので、由来のノードだけではキーにならない。
// 由来ごとに 1 つ決まる変換 (conditional / match / range) は表に載せるが、
// これは呼ぶ側の材料を受け取って組み立てる道具。
//
// バッファをどこから持ってくるか (どう宣言し、何バイト読んだか) は呼ぶ側の
// 仕事。lowering/conditional が宣言を合成しないのと同じ線引き。

namespace brgen::nast::lowering {

    // バイト順は呼ぶ側が渡す。型に綴られていない限り、実際の順は
    // `input.endian` のスコープで決まり、それは FieldEndian 表 (over Field)
    // にある。unspec を渡すと型に書かれたものを使い、それも無ければ big
    // (言語の既定)。
    //
    // 実行時に決まる場合 (FieldEndian の dynamic) はここでは扱わない。
    // 両方の順を出して選ばせる形が要る (EBM の add_endian_specific /
    // IS_LITTLE_ENDIAN に当たるもの)。

    // bytes[offset + 0..n) から値を組む式。type は IntType であること。
    // offset が null なら 0 から。符号つきなら最後にその型へ落とす。
    Node<Expr> combine_int(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Type> type,
                           Endian order = Endian::unspec);

    // 値を bytes[offset + 0..n) へ書く文の並び。
    Node<Body> split_int(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Expr> value,
                         Node<Type> type, Endian order = Endian::unspec);

}  // namespace brgen::nast::lowering
