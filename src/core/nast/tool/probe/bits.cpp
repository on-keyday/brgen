/*license*/
// `nast_probe bits` — バイト境界に乗らない field の並び (ビット群) と、
// それをまとめて読み書きする lowering (lowering/bit_group) の結果。
//
// 群の切り方は lowering の bit_groups そのもの。ここは閉じ方で数えるのと、
// 組めた群を .bgn に戻して印字するだけ。
#include "../../node/console.h"
#include "probe.hpp"
#include "../../lowering/bit_group.hpp"
#include "../../lowering/self_ref.hpp"
#include "../../node/build.h"
#include "../../node/util.h"
#include "../../parse/unparse.h"

#include <set>

namespace brgen::nast::probe {

    namespace {
        const char* close_name(lowering::BitGroup::Close c, bool format_body) {
            using C = lowering::BitGroup::Close;
            switch (c) {
                case C::flat:
                    return "平ら (Body 内で閉じる)";
                case C::branch:
                    return "閉じる前に分岐";
                case C::other_statement:
                    return "閉じる前に分岐以外の文";
                case C::variable_width:
                    return "閉じる前に可変幅";
                case C::body_end:
                    // format の Body の終わりで開いたままの群は、親の側で前後の
                    // field と合わさって閉じる (format の境界を跨ぐ)。分岐の腕で
                    // 開いたままのものは経路の続きで閉じる。解き方が違う。
                    return format_body ? "閉じる前に format の終わり" : "閉じる前に分岐の腕などの終わり";
            }
            return "?";
        }
    }  // namespace

    void run_bits(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        lowering::Context c{a, p.tables};
        std::set<std::uint32_t> format_bodies;
        each_node<Format>(a, [&](Node<Format> f) {
            format_bodies.insert(f.ref(a)->body.id());
        });
        // lowering が合成する Body を拾わないよう、範囲を先に固定する。
        auto last = a.node_count();
        each_node<Body>(a, last, [&](Node<Body> body) {
            for (auto& g : lowering::bit_groups(c, body)) {
                auto how = close_name(g.close, format_bodies.contains(body.id()));
                hist[std::string("群: ") + how]++;
                auto pos = g.fields.front().ref(a).loc();
                if (g.close != lowering::BitGroup::Close::flat) {
                    if (detail) {
                        print_line("{}:{}  {} field, {} bits  {}", pos.line + 1, pos.col + 1, g.fields.size(), g.bits, how);
                    }
                    continue;
                }
                Builder b{a, pos};
                std::vector<Node<Expr>> targets;
                for (auto f : g.fields) {
                    targets.push_back(name_of(a, f).empty() ? Node<Expr>{} : lowering::field_ref(c, f));
                }
                auto dec = lowering::lower_bit_group_decode(c, g, targets, b.ref("buf"), b.ref("o"));
                auto enc = lowering::lower_bit_group_encode(c, g, targets, b.ref("buf"), b.ref("o"));
                hist[dec && enc ? "平らな群: 組めた" : "平らな群: 組めない"]++;
                if (!dec || !enc) {
                    auto why = lowering::bit_group_refusal(c, g);
                    hist[std::format("  組めない理由: {}", why ? why : "(理由なし)")]++;
                }
                if (detail) {
                    print_line("{}:{}  {} field, {} bits  {}", pos.line + 1, pos.col + 1, g.fields.size(), g.bits, how);
                    if (!dec || !enc) {
                        auto why = lowering::bit_group_refusal(c, g);
                        print_line("  (組めない: {})", why ? why : "理由なし");
                        continue;
                    }
                    print_text("decode:\n{}\nencode:\n{}\n", unparse_node(a, dec), unparse_node(a, enc));
                }
            }
        });
    }

}  // namespace brgen::nast::probe
