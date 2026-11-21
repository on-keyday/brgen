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

#include <map>
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
        const char* end_name(lowering::BitRoute::End e) {
            using E = lowering::BitRoute::End;
            switch (e) {
                case E::closed:
                    return "閉じた";
                case E::variable_width:
                    return "可変幅";
                case E::loop:
                    return "ループ";
                case E::other_statement:
                    return "分岐以外の文";
                case E::body_end:
                    return "最初の Body の終わり";
                case E::too_long:
                    return "64 ビット超";
            }
            return "?";
        }

        // 分岐を跨ぐ群を経路で数える。全経路が閉じるか、長さが揃うか、
        // field ごとの位置が経路によらず 1 つに決まるか (決まるなら実行時の
        // カーソルが要らない)。
        void count_routes(lowering::Context& c, Node<Body> body, const lowering::BitGroup& g, bool detail, Hist& hist) {
            auto& a = c.a;
            auto& stmts = body.ref(a)->statements;
            std::size_t start = 0;
            while (start < stmts.size() && stmts[start] != g.fields.front()) {
                start++;
            }
            auto routes = lowering::bit_routes(c, body, start);
            std::map<std::string, std::size_t> ends;
            std::set<std::size_t> lengths;
            std::map<std::uint32_t, std::set<std::size_t>> offsets;
            bool all_closed = true;
            for (auto& r : routes) {
                ends[end_name(r.end)]++;
                if (r.end != lowering::BitRoute::End::closed) {
                    all_closed = false;
                    continue;
                }
                lengths.insert(r.bits);
                std::size_t pos = 0;
                for (auto f : r.fields) {
                    offsets[f.id()].insert(pos);
                    auto* sz = c.tables.table<TypeSize>().get(f.ref(a)->type);
                    pos += sz ? sz->bits : 0;
                }
            }
            if (!all_closed) {
                std::string why;
                for (auto& [k, v] : ends) {
                    if (k != "閉じた") {
                        why += (why.empty() ? "" : ", ") + k;
                    }
                }
                hist["経路: 閉じない経路がある"]++;
                hist["  閉じない理由: " + why]++;
            }
            else {
                bool one_offset = true;
                for (auto& [_, set] : offsets) {
                    one_offset = one_offset && set.size() == 1;
                }
                hist[std::format("経路: 全部閉じる ({}, {})", lengths.size() == 1 ? "長さ同じ" : "長さ違う",
                                 one_offset ? "位置が 1 つ" : "位置が経路で変わる")]++;
            }
            const char* why = nullptr;
            auto routed = lowering::routed_bit_group(c, body, start, &why);
            hist[routed ? "分岐を跨ぐ群: 組めた" : "分岐を跨ぐ群: 組めない"]++;
            if (!routed) {
                hist[std::format("  分岐を跨ぐ群の組めない理由: {}", why ? why : "(理由なし)")]++;
            }
            if (detail) {
                auto pos = g.fields.front().ref(a).loc();
                print_line("{}:{}  経路 {} 本", pos.line + 1, pos.col + 1, routes.size());
                for (auto& r : routes) {
                    std::string names;
                    for (auto f : r.fields) {
                        names += std::string(names.empty() ? "" : " ") + std::string(name_of(a, f));
                    }
                    print_line("    {} bits  {}  [{}]", r.bits, end_name(r.end), names);
                }
                if (!routed) {
                    print_line("  (組めない: {})", why ? why : "理由なし");
                    return;
                }
                print_line("  {} bytes まで、{}", routed->max_bytes, routed->lsb ? "lsb から" : "msb から");
                Builder b{a, pos};
                for (std::size_t i = 0; i < routed->fields.size(); i++) {
                    auto& f = routed->fields[i];
                    auto target = lowering::field_ref(c, f.field);
                    auto dec = lowering::lower_routed_bit_decode(c, *routed, i, target, b.ref("buf"), b.ref("o"));
                    auto enc = lowering::lower_routed_bit_encode(c, *routed, i, target, b.ref("buf"), b.ref("o"));
                    print_line("  --- {} @{} :{} bits  読む前に [{}, {})  書いた後に [{}, {})", name_of(a, f.field), f.offset, f.bits,
                               f.fetch_from, f.fetch_to, f.flush_from, f.flush_to);
                    print_text("  decode:\n{}\n  encode:\n{}\n", dec ? unparse_node(a, dec) : "(組めない)",
                               enc ? unparse_node(a, enc) : "(組めない)");
                }
            }
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
        // 対応表で平らな群に載った field。Body ごとに切った群と食い違うものを数える。
        std::map<std::uint32_t, const char*> placed;
        each_node<Format>(a, last, [&](Node<Format> f) {
            auto l = lowering::bit_layout(c, f);
            for (auto& [id, pl] : l.places) {
                placed[id] = pl.kind == lowering::BitPlace::Kind::flat     ? nullptr
                             : pl.kind == lowering::BitPlace::Kind::routed ? "分岐を跨ぐ群の一員"
                                                                          : pl.why;
            }
        });
        each_node<Body>(a, last, [&](Node<Body> body) {
            for (auto& g : lowering::bit_groups(c, body)) {
                auto how = close_name(g.close, format_bodies.contains(body.id()));
                hist[std::string("群: ") + how]++;
                auto pos = g.fields.front().ref(a).loc();
                if (g.close == lowering::BitGroup::Close::branch) {
                    count_routes(c, body, g, detail, hist);
                }
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
                if (dec && enc) {
                    auto it = placed.find(g.fields.front().id());
                    if (it == placed.end() || it->second) {
                        auto why = it == placed.end() ? "表に無い" : it->second;
                        hist[std::format("  Body ごとには組めたが表では外れる: {}", why)]++;
                        if (detail) {
                            print_line("{}:{}  Body ごとには組めたが表では外れる: {}", pos.line + 1, pos.col + 1, why);
                        }
                    }
                }
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
        // field と群の対応表。format ごとに、バイト境界に乗らない field が
        // 全部どこかの群に載るか。
        each_node<Format>(a, last, [&](Node<Format> f) {
            auto l = lowering::bit_layout(c, f);
            if (l.places.empty()) {
                hist["表 format: 群なし"]++;
                return;
            }
            hist[l.complete() ? "表 format: 全部載る" : "表 format: 組めない field あり"]++;
            hist["表 群: 平ら"] += l.flat.size();
            hist["表 群: 分岐を跨ぐ"] += l.routed.size();
            for (auto& [_, pl] : l.places) {
                switch (pl.kind) {
                    case lowering::BitPlace::Kind::flat:
                        hist["表 field: 平らな群"]++;
                        break;
                    case lowering::BitPlace::Kind::routed:
                        hist["表 field: 分岐を跨ぐ群"]++;
                        break;
                    case lowering::BitPlace::Kind::refused:
                        hist["表 field: 組めない"]++;
                        hist[std::format("  表 field の組めない理由: {}", pl.why ? pl.why : "(理由なし)")]++;
                        break;
                }
            }
            if (detail && !l.complete()) {
                print_line("format {}", name_of(a, f));
                visit_all(a, f.ref(a)->body, [&](NodeAny n) {
                    if (auto fld = n.as_any<Field>()) {
                        if (auto* pl = l.find(fld); pl && pl->kind == lowering::BitPlace::Kind::refused) {
                            auto pos = fld.ref(a).loc();
                            print_line("  {}:{}  {}  {}", pos.line + 1, pos.col + 1, name_of(a, fld), pl->why ? pl->why : "?");
                        }
                    }
                    return true;
                });
            }
        });
    }

}  // namespace brgen::nast::probe
