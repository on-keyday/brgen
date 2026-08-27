/*license*/
#pragma once
#include "../nodes.h"

#include <core/common/error.h>
#include <optional>
#include <set>

// 定数式を畳んで ConstantValue 表に置く。元の ast::tool::Evaluator に当たる段。
//
// 値の表現: integer は絶対値 (u64) で、符号は is_negative。u64 の全域と
// -(2^64-1) までを落とさず持てる。演算は符号付き 128bit でやって、
// 収まらないもの (オーバーフロー / ゼロ除算 / 範囲外のシフト量) は畳まない。
//
// 畳むもの:
//   リテラル      Int (0x 等の前置も) / Bool / Char / Str
//   単項          - (整数) と ! (bool)
//   二項          算術・シフト・ビット・比較・論理。比較は bool になる。
//                 シフトとビット演算は両辺が非負のときだけ
//   Paren / Identity / Cond (条件が畳めれば選ばれた側だけ見る)
//   Reference     ::= の定義と enum メンバ。:= は後から代入され得るので畳まない
//   MemberAccess  enum メンバ (Color.red)。import 先の ::= も Resolution 経由で同じ
//
// 畳まないもの: sizeof (レイアウトが要る)、input / config、fn 呼び出し。
// 畳めないことはエラーではない。定数でない式が大半で、それが正常。
//
// 表を引く側 (LSP の hover など) は「entry が有る = 定数」で見ればよい。

namespace brgen::nast::bind {

    struct Evaluator {
        Arena& a;
        SideTables& tables;
        LocationError& err;

        // 畳めた式の数。診断ではなく計測用。
        std::size_t evaluated = 0;

        void run(Node<Module> mod);

        // 以下は実装の内側。相互参照 (a ::= b, b ::= a) で戻ってきたら諦める。
        std::set<std::uint32_t> in_progress_;

        // 畳めれば表に置いて指す。畳めなければ nullptr。
        const ConstantValue* eval(Node<Expr> e);

        // 表の実体は伸びる vector なので、実装の中は値で受け渡す。
        std::optional<ConstantValue> value_of(Node<Expr> e);
        std::optional<ConstantValue> value_of_decl(Node<Statement> decl);
        std::optional<ConstantValue> compute(Node<Expr> e);
        std::optional<ConstantValue> compute_binary(Node<Binary> bin);
    };

}  // namespace brgen::nast::bind
