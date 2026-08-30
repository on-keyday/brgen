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
    // **ここで組めるのは big と little だけ。** 決まらない場合が 2 つあり、
    // どちらも null を返す:
    //
    //   native   ターゲット上の静的な値。生成器には決められないが、ターゲットの
    //            コンパイラには決められる (rust の cfg!(target_endian)、
    //            C++ の std::endian::native、C の #if BYTE_ORDER)
    //   dynamic  実行時の値 (FieldEndian の dynamic)。代入の位置で 1 度だけ
    //            評価した変数を読んで選ぶ
    //
    // 構造はどちらも同じで、「両方の順で組んだものと選択子を渡し、選ぶのは
    // 相手」になる。EBM の IS_LITTLE_ENDIAN が両方を兼ねているのもそのため。
    // その規則はまだ無い。

    // bytes[offset + 0..n) から値を組む式。type は IntType であること。
    // offset が null なら 0 から。符号つきなら最後にその型へ落とす。
    Node<Expr> combine_int(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Type> type,
                           Endian order = Endian::unspec);

    // 値を bytes[offset + 0..n) へ書く文の並び。
    Node<Body> split_int(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Expr> value,
                         Node<Type> type, Endian order = Endian::unspec);

    // 順が決まらないとき用。両方の形を組んで選択子で選ぶ。
    //
    //   combine: is_little ? <little で組んだ式> : <big で組んだ式>
    //   split:   if is_little: <little の代入> else: <big の代入>
    //
    // **選択子は呼ぶ側が作る。** ここで作れないものだからこの形になっている:
    //
    //   native   ターゲットのコンパイル時の判定。書き方は言語ごとに違う
    //            (rust `cfg!(target_endian = "little")` / C
    //            `(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)` / python
    //            `sys.byteorder == 'little'`)
    //   dynamic  代入の位置で 1 度だけ評価した変数との比較。どの値が little かは
    //            保持の仕方で決まる
    //
    // EBM も同じ形で、`add_endian_specific` が両方の枝を作り、`IS_LITTLE_ENDIAN`
    // の中身 (native なら knob の文字列、dynamic なら変数比較) を言語側に委ねて
    // いる。native と dynamic を 1 つの経路にまとめられるのは、違いが選択子の
    // 中身だけだから。
    Node<Expr> combine_int_either(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Type> type,
                                  Node<Expr> is_little);

    Node<Body> split_int_either(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Expr> value,
                                Node<Type> type, Node<Expr> is_little);

}  // namespace brgen::nast::lowering
