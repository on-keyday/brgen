/*license*/
#pragma once
#include "lowering.hpp"

// match を if の連鎖にする。match を持たない言語向け。
//
//   match x:          ->   if x == 1:
//       1   => A               A
//       2,3 => B           elif x == 2 || x == 3:
//       ..  => C               B
//                          else:
//                              C
//
// 比較は木に無い。parse.cpp は match の分岐を全部「条件つき
// ConditionalStatement」にするだけで、`x ==` の部分は作らない (条件なしの
// match `match:` では分岐条件がそのまま真偽値になる)。だからこの変換の実体は
// **比較を実体化すること**で、単なる詰め替えではない。
//
// `1,2 =>` は OrCond で来る (base にカンマ連結の Binary、conds に葉)。
// 比較は葉ごとに作って || で繋ぐ。
//
// 分岐の body は複製せず元のノードをそのまま指す。複製すると中の名前が
// Resolution 表に載っていない別ノードになり、解決先を失う。

namespace brgen::nast::lowering {

    // 由来の Match をキーに表へ置く。2 度呼んでも同じノードを返す。
    Node<If> lower_match(Context& c, Node<Match> match);

}  // namespace brgen::nast::lowering
