/*license*/
#pragma once
#include "../bind/pipeline.h"

#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 解析した木を見て回る口。ebmgen の interactive debugger
// (src/ebmgen/interactive/) に当たるもので、狙いも同じ — 「この id は何で、
// どこから来て、解析は何を書いたのか」を、印字プログラムを書き足さずに追う。
//
// **道具ではなく層に置いてある。** 対話は `nast_query` が入り口だが、中身は
// ここにあるので、バックエンドや LSP のような別の入り口からも同じ問い合わせが
// できる (ebmgen も debugger 本体と run_query を分けている)。出力は文字列に
// 積むだけで、どこへ出すかは呼ぶ側が決める。
//
//   query::Session s{program};
//   std::string out;
//   s.run("find Available", out);      // 1 行実行
//   query::repl(s, std::cin, std::cout);
//
// EBM 版との違いは、こちらは**原文に戻せる**こと。ノードは綴り (`u`) と
// 原文の位置 (`src`) の両方から見られる。

namespace brgen::nast::query {

    struct Session {
        Program& p;
        // 所有辺での親。`up` と、探した結果に文脈を出すのに使う。
        std::unordered_map<std::uint32_t, std::uint32_t> parent;
        // Module から辿れるもの。アリーナにはパーサが捨てた木も残っているので、
        // 「木に居るのか」は見えるようにしておく (孤児は 4 割ある)。
        std::unordered_set<std::uint32_t> reachable;

        explicit Session(Program& prog);

        // ---- 問い合わせ。対話を通さずに使える口 ------------------------------

        // 実行時の型を載せた参照。id しか無いところから木に入る。
        NodeAny node_at(std::uint32_t id) const;
        // 1 行の見出し。種別・位置・名前か綴りの先頭。
        std::string headline(std::uint32_t id) const;
        // ノードを木で。side table のエントリも一緒に出る。depth 0 で無制限。
        std::string print_node(std::uint32_t id, std::size_t depth) const;
        // 原文のその位置 (診断と同じ体裁)。
        std::string source_at(std::uint32_t id) const;
        // ノードでないフィールドを名前で引く。綴りは print_node と同じ。
        std::optional<std::string> field_text(std::uint32_t id, std::string_view name) const;
        // その種別のノード。key が空でなければ、その名前のフィールドが value の
        // ものだけ。文字列は引用符の有無どちらでも当たる。
        std::vector<std::uint32_t> find(NodeType kind, std::string_view key = {},
                                        std::string_view value = {}) const;

        // ---- 対話 ------------------------------------------------------------

        // 1 行を実行して out に積む。false を返したら終わり (quit)。
        bool run(std::string_view line, std::string& out) const;
        // コマンド一覧。
        static std::string help();
    };

    // 対話ループ。プロンプトは out へ、行は in から。
    void repl(const Session& s, std::istream& in, std::ostream& out);

}  // namespace brgen::nast::query
