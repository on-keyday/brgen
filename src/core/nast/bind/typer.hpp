/*license*/
#pragma once
#include "../nodes.h"

#include <core/common/error.h>
#include <set>
#include <string_view>

// 式に型を付ける。元の src/core/middle/typing.cpp (2076 行) に当たる段の一部。
//
// 今入っているのは「他の式の型を要らない」ところまで:
//
//   リテラル      IntLiteral -> IntLiteralType など。値から直に決まる
//   Reference     解決先 (Resolution 表) の型をそのまま持ってくる
//   Paren/Identity 中身の型
//   Import        読み込んだ Module の struct_type
//   IdentType     指している宣言の型を base に入れる
//   MemberAccess  左辺の型からメンバを引く
//
// 入っていないもの: Binary / Call / Cast / Index / If / Match / Cond / Range /
// OrCond / IOOperation。演算子ごとの規則や共通型の計算が要る。
//
// メンバの引き方は元の実装と違う。元は StructType がメンバ一覧 (fields) を
// 持っていたが、nast の StructType は base だけで、一覧は binder が
// FormatState / UnionFields 表に集めている。そこから引く。
//
// 型の付き具合は corpus driver が「typed/exprs」で出す。
//
// StructType は format / state / module ごとに 1 つで、要求されたときに作って
// 宣言側のフィールドに載せる。同じ format への参照が同じノードを指すようにする
// ためで、parse では作らない (構文からは決まらないため)。
//
// 解決先の型がまだ無ければ先にそちらを付けに行く。相互参照で戻ってくることが
// あるので、途中のものを in_progress_ で覚えて二度目は諦める。元の実装の
// recurse_detect に当たる。

namespace brgen::nast::bind {

    struct Typer {
        Arena& a;
        SideTables& tables;
        LocationError& err;

        // 型が付いた式と、付かなかった式。診断にはしない。付かない理由は
        // 「まだ実装していない種類」が大半で、入力の誤りとは限らない。
        std::size_t typed = 0;
        std::size_t untyped = 0;

        void run(Node<Module> mod);

        // 以下は実装の内側。
        std::set<std::uint32_t> in_progress_;

        Node<Type> type_of_expr(Node<Expr> e);
        Node<Type> type_of_decl(Node<Statement> decl);
        // 型の包み (IdentType など) を剥がして StructType を取り出す。
        Node<StructType> as_struct(Node<Type> t);
        // struct の持ち主 (format / state / module) から名前でメンバを引く。
        Node<Statement> lookup_member(Node<Statement> owner, std::string_view name);
        Node<Type> type_of_member_access(Node<MemberAccess> m);
        Node<Type> struct_type_of(Node<Statement> owner);
        Node<Type> struct_type_of_module(Node<Module> mod);
        void resolve_ident_type(Node<IdentType> t);
    };

}  // namespace brgen::nast::bind
