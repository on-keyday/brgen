/*license*/
// `nast_probe bits` — バイト境界に乗らない field の並び (ビット群) を数える。
//
// 同じ Body の中で連続していて、バイト境界から始まりバイト境界で閉じる群は
// CFG なしで 1 回の読み書きに畳める (wirespec の BitGroup と同じ条件)。
// 閉じる前に分岐・ループ・Body の終わりに当たる群は経路を辿らないと畳めない。
// その割合が CFG を先に作るかどうかの材料になる。
#include "../../node/console.h"
#include "probe.hpp"
#include "../../node/util.h"

#include <set>

namespace brgen::nast::probe {

    namespace {
        // 群を閉じずに通してよい文。読み書きをしないもの。
        bool is_transparent(NodeType t) {
            switch (t) {
                case NodeType::Assert:
                case NodeType::VariableDefinition:
                case NodeType::Metadata:
                    return true;
                default:
                    return false;
            }
        }
    }  // namespace

    void run_bits(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        auto& t = p.tables;
        // Body の持ち主。format の Body の終わりで開いたままの群は、親の側で
        // 前後の field と合わさって閉じる (format の境界を跨ぐ)。分岐の腕で
        // 開いたままのものは経路の続きで閉じる。解き方が違うので分けて数える。
        std::set<std::uint32_t> format_bodies;
        each_node<Format>(a, [&](Node<Format> f) {
            format_bodies.insert(f.ref(a)->body.id());
        });
        each_node<Body>(a, [&](Node<Body> body) {
            std::size_t open_bits = 0;   // 群の中で積んだ幅
            std::size_t members = 0;     // 群の field 数
            Node<Field> first{};
            auto close = [&](const char* how) {
                if (members == 0) {
                    return;
                }
                hist[std::string("群: ") + how]++;
                if (detail) {
                    auto pos = first.ref(a).loc();
                    print_line("{}:{}  {} field, {} bits  {}", pos.line + 1, pos.col + 1, members, open_bits, how);
                }
                open_bits = 0;
                members = 0;
                first = {};
            };
            for (auto& s : body.ref(a)->statements) {
                if (is_transparent(s.type())) {
                    continue;
                }
                auto fld = s.as<Field>();
                if (!fld) {
                    if (members) {
                        close(is_derived<ConditionalExpr>(s.type()) ? "閉じる前に分岐" : "閉じる前に分岐以外の文");
                    }
                    continue;
                }
                auto type = fld.ref(a)->type;
                auto* sz = type ? t.table<TypeSize>().get(type) : nullptr;
                if (!sz || sz->kind != SizeKind::fixed) {
                    if (members) {
                        close("閉じる前に可変幅");
                    }
                    continue;
                }
                if (members == 0) {
                    if (sz->bits % 8 == 0) {
                        continue;  // 普通の field
                    }
                    first = fld;
                }
                members++;
                open_bits += sz->bits;
                if (open_bits % 8 == 0) {
                    close("平ら (Body 内で閉じる)");
                }
            }
            close(format_bodies.contains(body.id()) ? "閉じる前に format の終わり" : "閉じる前に分岐の腕などの終わり");
        });
    }

}  // namespace brgen::nast::probe
