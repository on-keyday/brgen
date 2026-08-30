/*license*/
#pragma once
#include "lowering.hpp"

// 三項を文の形にする。三項を持たない言語 (や、腕に文を要する言語) 向け。
//
//   cond ? then : els
//     ->  <type> tmpN            宣言は呼ぶ側が書く (後述)
//         if cond: tmpN = then
//         else:    tmpN = els
//         ... tmpN ...
//
// **宣言は合成しない。** nast の VariableDefinition は初期化子から型を取る
// ので、値なしの宣言が書けない。EBM は「型の既定値」ノードを持っていて
// `tmp := default(T)` を作れるが、nast の語彙にはそれが無い。そして宣言の
// 書き方は言語ごとに違う (Go は `var x T`、C は `T x;`、Rust は `let x;` で
// definite assignment) ので、既定値を発明して押し付けるより、型と名前を
// 返して宣言はバックエンドに書かせるほうが素直。
//
// 返すもの:
//   temp_name  生成した一時変数の名前
//   type       その型 (then と els の共通型。呼ぶ側が宣言に使う)
//   branch     tmpN への代入を含む分岐。宣言の直後に置く
//   value      元の式の代わりに置く参照

namespace brgen::nast::lowering {

    // 由来の Cond をキーに表へ置く。2 度呼んでも同じノードを返す。
    // then/els の共通型が出せないときは null を返す (型が付いていない式)。
    LoweredCond* lower_conditional(Context& c, Node<Cond> cond);

}  // namespace brgen::nast::lowering
