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
//   Binary        演算子ごと。比較は bool、算術は共通型、代入は左辺の型
//   Unary         中身の型 (- と ! はどちらも型を変えない)
//   Index         base の配列型の要素型
//   Range         RangeType。基底は start か end の型
//   Cast          変換先の型
//   Call          呼ぶ先の FunctionType の戻り値
//   If/Match/Cond 分岐の型が揃えばその型、揃わなければ void
//   Sizeof        u64
//   Available     bool
//
//   SpecialLiteral input / output は StreamType。素のものは length 無し
//                 (どの実体で呼ばれるかは format からは決まらない)。
//                 input.subrange(len) の値は length に len を確立した StreamType。
//                 get / peek は引数 0 の型リテラルが名指す型 (無ければ u8)、
//                 offset / bit_offset / remain / scope_length は u64、
//                 backward / put は void。type_of_stream_call を見よ。
//
// 入っていないもの:
//   OrCond                   match の分岐条件を | でつないだ形
//   SpecialLiteral の config  ストリームではなく自由なメタデータ名前空間
//                            (config.url = "..") なので別扱いが要る
//
// input.endian など表に無いストリームのメンバは型なしのまま通す。能力要求の
// 検査 (peek を使う format は先読み可能な入力を要求する、の類) も意図して
// 入れていない。まず型として使い方を見てから締める。元実装の
// resolve_io_operation + IOOperation 置き換えに当たる段は nast には作らず、
// 型付けだけで済ませている。
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
        // for x in c で x に見える型。c の型から決まる。
        Node<Type> iteration_type(Node<Type> container_type);
        // 型の包み (IdentType など) を剥がして StructType を取り出す。
        Node<StructType> as_struct(Node<Type> t);
        // struct の持ち主 (format / state / module) から名前でメンバを引く。
        Node<Statement> lookup_member(Node<Statement> owner, std::string_view name);
        Node<Type> type_of_member_access(Node<MemberAccess> m);
        // input.get(u8) などストリームの組み込みメソッド。該当しなければ nullref。
        Node<Type> type_of_stream_call(Node<Call> call);
        Node<Type> type_of_binary(Node<Binary> b);
        Node<Type> type_of_conditional(Node<ConditionalExpr> c);
        // ブロックの値。最後の文が式ならその型。
        Node<Type> block_value_type(Node<Body> body);
        // 整数リテラルの型を相手の整数型に合わせる。合わないときは何もしない。
        // 元の int_type_fitting に当たるが、型ノードを書き換えずに結果を返す。
        Node<Type> fit_int(Node<Type> t, Node<Type> other);
        // 両方に使える型。整数どうしは幅と符号で決める。
        Node<Type> common_type(Node<Type> l, Node<Type> r);
        Node<Type> struct_type_of(Node<Statement> owner);
        Node<Type> struct_type_of_module(Node<Module> mod);
        void resolve_ident_type(Node<IdentType> t);
    };

}  // namespace brgen::nast::bind
