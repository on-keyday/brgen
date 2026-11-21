/*license*/
#pragma once
#include "lowering.hpp"

#include <optional>
#include <string>

// field の後ろに何が来るか。長さの書かれていない配列 (`[..]u8`) をどこまで
// 読むかはこれで決まる:
//
//   end       後ろに何も無い             入力の終わりまで
//   fixed     後ろは幅の決まった field    eventual が end なら、残りから後ろの
//                                         固定幅 (tail_bits) を除いたところまで
//   constant  後ろは文字列の期待値        その値 (終端) が現れるところまで
//   normal    後ろは幅の決まらないもの    決められない
//
// 元実装の middle (type_attribute.cpp の Follow / eventual_follow /
// fixed_tail_size) と同じ規則。ebmgen の convert/decode.cpp はこれを見て
// 配列の読み方を変えている。違いは単位で、元実装は struct ごと (分岐の腕は
// 別の struct)、ここは Body ごと。分岐 (If / Match) は幅の決まらないもの
// として数える。
//
// 解析の段の表にせず、呼ばれたときに Body を見て答える。答えは
// Body の並びと TypeSize だけから決まる。

namespace brgen::nast::lowering {

    struct FieldFollow {
        enum class Kind : std::uint8_t {
            end,
            fixed,
            constant,
            normal,
        };
        Kind kind = Kind::end;
        // 後ろで最初に来る固定幅でないもの (end / constant / normal)。
        Kind eventual = Kind::end;
        // eventual が end のとき、後ろの固定幅の field の合計ビット数。
        std::optional<std::size_t> tail_bits;
        // すぐ後ろの field (constant ならその終端)。end なら null。
        Node<Field> next;
    };

    // body の statements[index] にある field について。
    FieldFollow field_follow(Context& c, Node<Body> body, std::size_t index);

    // 文字列の期待値の型 (`magic :"ustar\0"`) が表すバイト列。
    std::optional<std::string> literal_bytes(Context& c, Node<Type> t);

}  // namespace brgen::nast::lowering
