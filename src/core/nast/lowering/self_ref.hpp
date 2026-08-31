/*license*/
#pragma once
#include "lowering.hpp"

// レシーバ (self / this / Go の t のような名前) の扱い。
//
// **原文の木にレシーバは無い。** `data :[len]u8` の `len` は裸の Reference で、
// 解決先が Field であることが Resolution 表に載っているだけ。生成コードでは
// `t.Len` のようにレシーバを付けないといけないが、「ここに要る」という印が
// 木のどこにも無い。
//
// **付けるのは綴る側。lowering は付けない。** 原文から来た式は書き換えられ
// ないので (複製すると中の名前が Resolution 表に載っていない別ノードになり
// 解決先を失う)、lowering が作る参照にだけレシーバを付けると、1 つの式の中で
// 同じ意味のものが 2 つの形になる。読む側が両方を扱う羽目になるので、
// **形は 1 つに揃えて裸の参照にし、レシーバは綴る側が 1 つの規則で足す**:
// 参照の解決先が Field なら前置する、以上。
//
// EBM は変換の時点で `MEMBER_ACCESS{base: SELF}` に実体化している。あちらは
// 変換が式を作り直す立場なので揃えられる。こちらは原木を残す立場なので、
// 揃える先が逆になる。綴りが言語ごとなのは同じ (`MEMBER_ACCESS` は共通化
// 不適と測定済みのグループ)。
//
// EBM が変換時に実体化したのはレシーバが一定でないためで、スコープの
// スタック + 宣言ごとの表 (`self_ref_map`) の 2 段になっている。nast では
// そのうち 3 つ (state variable / 関数ローカル / member 側) が別のノード種に
// なっていて起きない。残るのは分岐の中の field の保存場所で、平らに持つ間は
// `self.name` で足りる — 詳細は docs/size_and_lowering.md の
// 「ebmgen のレシーバ機構」。
//
// **`available` の解決はこの区別を要る。** `available(field)` (裸) と
// `available(a.b.field)` (修飾) では、gating 条件を別の base に載せ直すか
// どうかが変わる。EBM は変換で裸の Ident も `MEMBER_ACCESS{base: SELF}` に
// してしまうので、変換後の形を見ると裸まで修飾と判定してしまい、変換前の
// AST の形を見に行っている (ebmgen/convert/expression.cpp の
// convert_expr_impl(Available))。木にレシーバを実体化しないというのは、その
// 区別を最後まで残すということでもある — **available の target にレシーバを
// 足してはいけない**。

namespace brgen::nast::lowering {

    // その field を指す式。解決先も表に入れる。**レシーバは付けない** —
    // 原文の参照と同じ形にするため。
    Node<Expr> field_ref(Context& c, Node<Field> f);

    // その参照が指しているのが field なら、その field。レシーバが要るかどうかの
    // 判定に使う。綴る側が呼ぶ。
    Node<Field> receiver_field(Context& c, Node<Reference> ref);

    // レシーバそのもの。綴る側がノードとして扱いたいときに使う。
    // owner を渡すとその struct 型が付く。
    Node<Expr> self_ref(Context& c, Node<Format> owner, lexer::Loc loc);

}  // namespace brgen::nast::lowering
