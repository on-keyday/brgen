/*license*/
#pragma once
#include "nodes.h"

#include "error.h"
#include <code/loc_writer.h>
#include <cstddef>
#include <concepts>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// テキストを書き出すときの土台。.bgn へ書き戻す側 (parse/unparse) と、
// これから作るバックエンド (各言語のコードを出す側) の両方が使う。
//
// futils の LocWriter に載せる。素の文字列連結と違うのは 3 つ:
//
//   - 行とインデントを構造として持つ。桁を数えなくてよく、インデントは
//     indent_scope() のスコープで上下する
//   - Writer どうしを merge できる。部分を別に組み立てて後で貼れる
//   - **どの断片がどのノードから出たかを記録できる** (with_loc_scope)。
//     この対応表が source map になる
//
// 3 つ目が本命で、EBM で「生成コードから元の .bgn へ戻れない」と分かった
// ことへの備え (docs/exit_and_reversibility.md)。バックエンドを書くときは、
// 出力の単位ごとに with_loc_scope を張っておくこと。後から足すのは難しい。
//
// インデントの幅は書き出す言語ごとに違う (4 / 2 / タブ)。Writer 自身は
// 深さしか持たないので、幅は to_string / adjust_with_indent に渡す。
// IndentStyle にまとめてあるので、そのまま持ち回せばよい。

namespace brgen::nast {

    // どのノードが出したかを覚える Writer。
    using CodeWriter = futils::code::LocWriter<std::string, std::vector, NodeAny>;

    struct IndentStyle {
        std::string text = "    ";
        std::size_t width = 4;

        static IndentStyle spaces(std::size_t n) {
            return IndentStyle{std::string(n, ' '), n};
        }

        // タブは桁の数え方が環境で変わるので、幅は 1 と数える
        // (spans の桁も同じ規約になる)。
        static IndentStyle tab() {
            return IndentStyle{"\t", 1};
        }
    };

    // 出力の断片と、それを出したノードの対応。行は 1 起点、桁は 0 起点で、
    // どちらもインデントを展開した後 (= text の上でそのまま切り出せる) 値。
    struct CodeSpan {
        NodeAny node;
        std::size_t begin_line = 0;
        std::size_t begin_col = 0;
        std::size_t end_line = 0;
        std::size_t end_col = 0;
    };

    struct CodeOutput {
        std::string text;
        std::vector<CodeSpan> spans;
    };

    // Writer の中身をテキストと対応表に落とす。Writer が持つ桁はインデントを
    // 展開する前のものなので、ここで style の幅を足して合わせる。
    inline CodeOutput finish(const CodeWriter& w, IndentStyle style = {}) {
        CodeOutput out;
        out.text = w.to_string(style.text.c_str());
        for (auto& loc : w.locs_data()) {
            auto adjusted = w.adjust_with_indent(style.width, loc);
            out.spans.push_back(CodeSpan{
                .node = adjusted.loc,
                .begin_line = adjusted.start.line,
                .begin_col = adjusted.start.pos,
                .end_line = adjusted.end.line,
                .end_col = adjusted.end.pos,
            });
        }
        return out;
    }

    // ブロックの深さを積む。書き出しは再帰で降りるので、indent_scope が返す
    // defer をスコープに置けない (入るところと出るところが別の関数になる)。
    // 積んでおいて明示的に解く。
    template <class W = CodeWriter>
    struct IndentStackOf {
        W& w;
        std::vector<decltype(std::declval<W&>().indent_scope())> scopes;

        explicit IndentStackOf(W& w)
            : w(w) {}

        void enter() {
            scopes.push_back(w.indent_scope());
        }

        void leave() {
            scopes.pop_back();
        }
    };

    using IndentStack = IndentStackOf<CodeWriter>;

    // ---- 断片を組み立てる ------------------------------------------------
    //
    // 出力を「その場で 1 つの Writer に足す」のではなく、小さい Writer を
    // 作って返し、呼んだ側が貼る形にするためのもの。式のように入れ子で
    // 組み上がるものは、返り値で持ち回るほうが順序を間違えない。
    // rebrgen の ebmcodegen (stub/util.hpp) から移した。
    //
    //   CODE("if ", cond, " {")       断片を 1 つ作る
    //   CODELINE(...)                 行を終える版
    //   SEPARATED(", ", args)         区切り文字で繋ぐ
    //   TRY_SEPARATED(", ", args, f)  各要素の組み立てが失敗しうる版
    //
    // 位置を渡す形 (CODE_AT / CODELINE_AT) は、その断片をどのノードが出したか
    // を記録する。バックエンドの出力は基本こちらで書く。

    template <class T>
    concept has_to_writer = requires(T t) {
        { t.to_writer() };
    };

    namespace internal {

        // 間違えやすいものはコンパイル時に止める。expected は中身を取り出す
        // 前に渡されがちで、整数は文字列に見えて数値のまま入ってしまう。
        template <class... A>
        constexpr void check_writable() {
            static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<A>, futils::helper::either::expected>),
                          "expected<> cannot be written directly; unwrap it first");
            static_assert(!(... || std::is_integral_v<std::decay_t<A>>),
                          "integers are not written as text; convert to string first");
        }

    }  // namespace internal

    // 断片を 1 つ作る。
    inline auto code_write(auto&&... args) {
        internal::check_writable<decltype(args)...>();
        CodeWriter w;
        w.write(std::forward<decltype(args)>(args)...);
        return w;
    }

    // 断片を作り、どのノードから出たかを記録する。
    inline auto code_write_at(NodeAny loc, auto&&... args) {
        internal::check_writable<decltype(args)...>();
        CodeWriter w;
        w.write_with_loc(loc, std::forward<decltype(args)>(args)...);
        return w;
    }

    inline auto code_writeln(auto&&... args) {
        internal::check_writable<decltype(args)...>();
        CodeWriter w;
        w.writeln(std::forward<decltype(args)>(args)...);
        return w;
    }

    inline auto code_writeln_at(NodeAny loc, auto&&... args) {
        internal::check_writable<decltype(args)...>();
        CodeWriter w;
        w.writeln_with_loc(loc, std::forward<decltype(args)>(args)...);
        return w;
    }

    // 並びを区切り文字で繋ぐ。要素は文字列でも Writer でもよい。
    inline auto code_join(auto&& joint, auto&& container) {
        CodeWriter w;
        bool first = true;
        for (auto&& v : container) {
            if (!first) {
                w.write(joint);
            }
            first = false;
            w.write(v);
        }
        return w;
    }

    // 個数と、番号から断片を作る関数で繋ぐ。
    inline auto code_join_n(auto&& joint, std::size_t count, auto&& fn) {
        CodeWriter w;
        for (std::size_t i = 0; i < count; i++) {
            if (i != 0) {
                w.write(joint);
            }
            if constexpr (std::is_invocable_v<decltype(fn), std::size_t>) {
                w.write(fn(i));
            }
            else {
                w.write(fn);
            }
        }
        return w;
    }

    // 要素ごとの組み立てが失敗しうる版。fn は brgen::result<> を返す。
    // 1 つでも失敗したらそこで止めてその失敗を返す。`bool first` のループを
    // 手で書くとこの中断を忘れるので、失敗しうるときはこちらを使う。
    template <class C, class F>
    expected<CodeWriter> try_code_join(auto&& joint, C&& container, F&& fn) {
        CodeWriter w;
        bool first = true;
        for (auto&& elem : container) {
            auto part = fn(elem);
            if (!part) {
                return brgen::nast::unexpect(part.error());
            }
            if (!first) {
                w.write(joint);
            }
            first = false;
            if constexpr (has_to_writer<decltype(*part)>) {
                w.write(part->to_writer());
            }
            else {
                w.write(std::move(*part));
            }
        }
        return w;
    }

    // 条件式の外側の括弧を落とす。`(a + b)` は `a + b` にするが、
    // `(1 + 1) + (2 + 2)` のように外側が対応していないものは触らない。
    // 括弧を自分で足す言語で、二重にならないようにするためのもの。
    inline std::string strip_outer_paren(std::string s) {
        if (!s.starts_with("(") || !s.ends_with(")")) {
            return s;
        }
        std::size_t level = 0;
        for (std::size_t i = 0; i < s.size(); i++) {
            if (s[i] == '(') {
                level++;
            }
            else if (s[i] == ')') {
                level--;
                if (level == 0) {
                    // 最後の 1 文字で閉じたときだけ、全体を包む括弧だった
                    return i == s.size() - 1 ? s.substr(1, s.size() - 2) : s;
                }
            }
        }
        return s;
    }

}  // namespace brgen::nast

// 短く書くための別名。マクロなので名前空間に属さない。
#define CODE(...) (::brgen::nast::code_write(__VA_ARGS__))
#define CODELINE(...) (::brgen::nast::code_writeln(__VA_ARGS__))
#define CODE_AT(loc, ...) (::brgen::nast::code_write_at((loc), __VA_ARGS__))
#define CODELINE_AT(loc, ...) (::brgen::nast::code_writeln_at((loc), __VA_ARGS__))
#define SEPARATED(...) (::brgen::nast::code_join(__VA_ARGS__))
#define TRY_SEPARATED(...) (::brgen::nast::try_code_join(__VA_ARGS__))
