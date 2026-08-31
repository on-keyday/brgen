/*license*/
#pragma once
#include "lowering.hpp"

// `available(x)` を式に落とす。
//
// 分岐の中で宣言された field は、その分岐を通ったときだけ存在する。
// `available(x)` はそれを訊く述語で、答えは **その field を宣言した分岐の
// 条件そのもの**:
//
//     match kind:
//         1 => value :u8
//         2 => value :u16
//
//     available(value)        ->  kind == 1 ? true : (kind == 2 ? true : false)
//     available(value, u16)   ->  kind == 1 ? false : (kind == 2 ? true : false)
//
// 第 2 引数の型は「今どちらの候補か」を訊く形 (example/coap.bgn の
// `available(extended_option_delta, u8)`)。候補の型が一致する分岐だけが真に
// なる。**ebmgen はこの引数を見ていない** (`expected_type` の参照が
// rebrgen に 1 つも無い) ので、あちらでは u8 と u16 の問いが同じ式になる。
//
// 材料は binder が置いた UnionType。分岐ごとに宣言された同名 field を
// 1 つの Field にまとめるとき、候補 (UnionCandidate) に「その分岐の条件」と
// 「その分岐にその field があるか (無ければ null)」を並べてある。つまり
// available に要るものは既に木にある — ここの仕事は畳み方だけ。
//
// 分岐の外で宣言された field は常にある。UnionType でなければ `true`。
//
// **修飾された target (`lab.pointer`) はまだ落とせない。** 候補の条件は
// 内側の format の field を指していて、それを綴ると self に対する参照に
// なってしまう (`(*this).flag`)。ほしいのは `(*this).lab.flag` で、条件式を
// `lab` に載せ直す必要がある。EBM は変換器の状態 (`set_self_ref`) で切り
// 替えているが、nast は原木を残す立場なので、載せ替えの表し方 (ノードを
// 足すか、参照を作り直せる複製器を用意するか) が未決。corpus では 11 件中
// 2 件 (dns.bgn)。

namespace brgen::nast::lowering {

    // 落とせないときは null。
    Node<Expr> lower_available(Context& c, Node<Available> av);

}  // namespace brgen::nast::lowering
