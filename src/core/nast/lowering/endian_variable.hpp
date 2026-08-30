/*license*/
#pragma once
#include "lowering.hpp"

// 実行時に決まるバイト順を、代入の位置で 1 つの変数に落とす。
//
//   input.endian = order.is_big ? config.endian.big : config.endian.little
//     ->  endian<id> = <その式>          代入の位置に置く (呼ぶ側が置く)
//         ... is_little_endian(..) が endian<id> を読む
//
// 式は代入の位置で 1 度だけ評価されるものなので、field ごとに展開しては
// いけない (参照している field が先に進んでいれば別の答えになるし、同じ式を
// field の数だけ焼くことになる)。EBM が ENDIAN_VARIABLE という文に落として
// いるのと同じ理由。
//
// 名前は由来のノード番号から作る (lowering/conditional の tmp<id> と同じ)。
// 「誰が名付けたか」を引く仕組みは要らない — 由来が決まれば名前も決まる。
//
// 宣言は組まない。型と初期値は分かっているが、宣言の書き方は言語ごとに違う
// (conditional と同じ線引き)。名前と値を返すので、置くのは呼ぶ側。

namespace brgen::nast::lowering {

    // 由来の SpecifyOrder をキーに表へ置く。2 度呼んでも同じものを返す。
    // `input.endian` 以外の指定 (bit_order 系) には作らない。
    LoweredEndianVariable* lower_endian_variable(Context& c, Node<SpecifyOrder> order);

}  // namespace brgen::nast::lowering
