/*license*/
#pragma once
#include "lowering.hpp"

// レシーバ (self / this / Go の t のような名前) の扱い。
//
// **原文の木にレシーバは無いが、束縛の段で実体化する。** `data :[len]u8` の
// `len` は parse の時点では裸の `Reference` で、解決先が `Field` であることが
// `Resolution` 表に載っているだけ。`bind/receiver` がそれを
// `MemberAccess{base: Self, member: 同じ Ident}` に差し替えるので、lowering と
// バックエンドが見る木では field 参照にレシーバが付いている。理由と境界は
// bind/receiver.hpp に書いてある。
//
// **ここで作る参照も同じ形にする。** 原文から来た参照と合成した参照が別の形に
// なると、1 つの式の中に同じ意味のものが 2 つ並ぶ:
//
//   ((8 + bit_sizeof(self.items)) + 64) + (8 * n)     ← 一度こうなった
//                       ^^^^ 合成                ^ 原文
//
// 読む側が両方を扱う羽目になるので、`field_ref` は実体化後と同じ形を返す。
//
// **腕の経路は作らない。** `Self` が言うのは「どの format のインスタンスか」まで。
// 分岐の中の field を腕の struct に入れるかどうかは格納戦略 = バックエンドの
// 選択で、そこは `UnionLayout` を見て決める (docs/size_and_lowering.md)。

namespace brgen::nast::lowering {

    // その field を指す式。解決先も表に入れる。format / state の field なら
    // レシーバ越し (`MemberAccess{Self, 名前}`)、関数の中で宣言されたものなら
    // 裸の参照 — bind/receiver と同じ規則。
    Node<Expr> field_ref(Context& c, Node<Field> f);

    // レシーバそのもの。綴る側がノードとして扱いたいときに使う。
    // owner を渡すとその struct 型が付く。
    Node<Expr> self_ref(Context& c, Node<Format> owner, lexer::Loc loc);

}  // namespace brgen::nast::lowering
