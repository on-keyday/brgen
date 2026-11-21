/*license*/
#pragma once
#include "lowering.hpp"

// `x.is_defined` — その値が列挙に並んでいるもののどれかか。
// EBM の `ENUM_IS_DEFINED` に当たる。
//
//   enum Hello: A = 1 / B = 2
//   test.is_defined   ->  (test == Hello.A) || (test == Hello.B)
//
// 綴りは値ではなくメンバの名前で出す。再 parse できる形になるのと、
// 生成コードでも名前で比べるほうが読めるため。並びが連続していれば範囲比較に
// 畳めるが、それは綴る側の判断なので here ではやらない (範囲比較を native に
// 書ける言語があるのと同じ理由、ADR 0035 の趣旨)。
//
// **base は比べる数だけ綴りに出る。** 一時変数に束ねたいときは呼ぶ側が
// 置き場を持つ (lowering/lowering.hpp の規約)。

namespace brgen::nast::lowering {

    // `x.is_defined` でなければ空。
    Node<Expr> lower_enum_is_defined(Context& c, Node<MemberAccess> ma);

}  // namespace brgen::nast::lowering
