/*license*/
#pragma once
#include "nodes.h"

#include <code/loc_writer.h>
#include <cstddef>
#include <string>
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

}  // namespace brgen::nast
