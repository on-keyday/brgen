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
                    return "flat (closes inside the Body)";
                case C::branch:
                    return "a branch before it closes";
                case C::other_statement:
                    return "a non-branch statement before it closes";
                case C::variable_width:
                    return "a variable width before it closes";
                case C::body_end:
                    // format の Body の終わりで開いたままの群は、親の側で前後の
                    // field と合わさって閉じる (format の境界を跨ぐ)。分岐の腕で
                    // 開いたままのものは経路の続きで閉じる。解き方が違う。
                    return format_body ? "the format ends before it closes" : "a branch arm (or similar) ends before it closes";
            }
            return "?";
        }
        const char* end_name(lowering::BitRoute::End e) {
            using E = lowering::BitRoute::End;
            switch (e) {
                case E::closed:
                    return "closed";
                case E::variable_width:
                    return "variable width";
                case E::loop:
                    return "loop";
                case E::other_statement:
                    return "non-branch statement";
                case E::body_end:
                    return "end of the first Body";
                case E::too_long:
                    return "over 64 bits";
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
                    if (k != end_name(lowering::BitRoute::End::closed)) {
                        why += (why.empty() ? "" : ", ") + k;
                    }
                }
                hist["routes: some route does not close"]++;
                hist["  why a route does not close: " + why]++;
            }
            else {
                bool one_offset = true;
                for (auto& [_, set] : offsets) {
                    one_offset = one_offset && set.size() == 1;
                }
                hist[std::format("routes: all close ({}, {})", lengths.size() == 1 ? "same length" : "lengths differ",
                                 one_offset ? "one position per field" : "positions vary by route")]++;
            }
            const char* why = nullptr;
            auto routed = lowering::routed_bit_group(c, body, start, &why);
            hist[routed ? "routed group: built" : "routed group: refused"]++;
            if (!routed) {
                hist[std::format("  why a routed group is refused: {}", why ? why : "(no reason)")]++;
            }
            if (detail) {
                auto pos = g.fields.front().ref(a).loc();
                print_line("{}:{}  {} route(s)", pos.line + 1, pos.col + 1, routes.size());
                for (auto& r : routes) {
                    std::string names;
                    for (auto f : r.fields) {
                        names += std::string(names.empty() ? "" : " ") + std::string(name_of(a, f));
                    }
                    print_line("    {} bits  {}  [{}]", r.bits, end_name(r.end), names);
                }
                if (!routed) {
                    print_line("  (refused: {})", why ? why : "no reason");
                    return;
                }
                print_line("  up to {} bytes, {}", routed->max_bytes, routed->lsb ? "from lsb" : "from msb");
                Builder b{a, pos};
                for (std::size_t i = 0; i < routed->fields.size(); i++) {
                    auto& f = routed->fields[i];
                    auto target = lowering::field_ref(c, f.field);
                    auto dec = lowering::lower_routed_bit_decode(c, *routed, i, target, b.ref("buf"), b.ref("o"));
                    auto enc = lowering::lower_routed_bit_encode(c, *routed, i, target, b.ref("buf"), b.ref("o"));
                    print_line("  --- {} @{} :{} bits  fetch before reading [{}, {})  flush after writing [{}, {})", name_of(a, f.field), f.offset, f.bits,
                               f.fetch_from, f.fetch_to, f.flush_from, f.flush_to);
                    print_text("  decode:\n{}\n  encode:\n{}\n", dec ? unparse_node(a, dec) : "(refused)",
                               enc ? unparse_node(a, enc) : "(refused)");
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
                             : pl.kind == lowering::BitPlace::Kind::routed ? "member of a routed group"
                                                                          : pl.why;
            }
        });
        each_node<Body>(a, last, [&](Node<Body> body) {
            for (auto& g : lowering::bit_groups(c, body)) {
                auto how = close_name(g.close, format_bodies.contains(body.id()));
                hist[std::string("group: ") + how]++;
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
                hist[dec && enc ? "flat group: built" : "flat group: refused"]++;
                if (dec && enc) {
                    auto it = placed.find(g.fields.front().id());
                    if (it == placed.end() || it->second) {
                        auto why = it == placed.end() ? "not in the table" : it->second;
                        hist[std::format("  built per Body but left out of the table: {}", why)]++;
                        if (detail) {
                            print_line("{}:{}  built per Body but left out of the table: {}", pos.line + 1, pos.col + 1, why);
                        }
                    }
                }
                if (!dec || !enc) {
                    auto why = lowering::bit_group_refusal(c, g);
                    hist[std::format("  why refused: {}", why ? why : "(no reason)")]++;
                }
                if (detail) {
                    print_line("{}:{}  {} field, {} bits  {}", pos.line + 1, pos.col + 1, g.fields.size(), g.bits, how);
                    if (!dec || !enc) {
                        auto why = lowering::bit_group_refusal(c, g);
                        print_line("  (refused: {})", why ? why : "no reason");
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
                hist["table format: no group"]++;
                return;
            }
            hist[l.complete() ? "table format: every field placed" : "table format: some field refused"]++;
            hist["table group: flat"] += l.flat.size();
            hist["table group: routed"] += l.routed.size();
            for (auto& [_, pl] : l.places) {
                switch (pl.kind) {
                    case lowering::BitPlace::Kind::flat:
                        hist["table field: flat group"]++;
                        break;
                    case lowering::BitPlace::Kind::routed:
                        hist["table field: routed group"]++;
                        break;
                    case lowering::BitPlace::Kind::refused:
                        hist["table field: refused"]++;
                        hist[std::format("  why a table field is refused: {}", pl.why ? pl.why : "(no reason)")]++;
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
