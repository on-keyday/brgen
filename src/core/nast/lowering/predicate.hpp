/*license*/
#pragma once
#include "lowering.hpp"

// 分岐のパターンを述語にする。
//
//   match x:            パターン        述語
//       1   => ...      1               x == 1
//       2,3 => ...      OrCond(2,3)     x == 2 || x == 3
//       4..8 => ...     Range(4,8)      x >= 4 && x < 8   (`..` は右開き)
//       4..=8 => ...    Range(4,8,=)    x >= 4 && x <= 8
//       ..  => ...      Range(,)        既定 (呼ぶ側が else にする)
//
// 木にはこの比較が無い。parse.cpp は分岐の条件としてパターンをそのまま置く
// だけで、`x ==` も範囲の展開も作らない。だから「比較の実体化」がここの
// 仕事で、詰め替えではない。EBM でいうと RANGE_EQUAL + lowered_expr
// (ADR 0035) に当たる部分を、式として組み立てる側に寄せた形。
//
// 条件なし match (`match:` で分岐条件が最初から真偽値) では subject が無い。
// そのときはパターンがそのまま述語になる。
//
// 範囲は端が片方だけのこともある (`4..` / `..8`)。無い側の比較は作らない。

namespace brgen::nast::lowering {

    // 作れないとき (パターンが無い) は null を返す。
    Node<Expr> branch_predicate(Context& c, Node<Expr> subject, Node<Expr> pattern);

    // 範囲との比較を普通の比較に展開する。
    //
    //   a == (1..=10)   ->   a >= 1 && a <= 10
    //   a != (20..30)   ->   !(a >= 20 && a < 30)
    //
    // 木は `Binary{equal, a, Paren{Range}}`。範囲が比較の片側に素で載る。
    //
    // **展開するかどうかはバックエンドが決める。** 範囲メンバーシップを直に
    // 書ける言語 (Rust の `(a..=b).contains(&x)`) はそのまま出したほうがよく、
    // 展開してしまうと「これは範囲判定だった」という意味が消えて native 表現
    // を選べない。EBM が RANGE_EQUAL を専用ノードにして lowered_expr を
    // 添えているのはそのため (ADR 0035)。nast は元の Binary をそのまま残し、
    // 展開が要る言語だけがこれを呼ぶ — 意味は木に残ったままになる。
    //
    // 範囲との比較でなければ null。
    Node<Expr> lower_range_compare(Context& c, Node<Binary> cmp);

}  // namespace brgen::nast::lowering
