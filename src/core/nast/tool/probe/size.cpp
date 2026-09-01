/*license*/
// `nast_probe size` — 型の幅と、実行時に決まる幅の式。
#include "probe.hpp"
#include "../../node/util.h"
#include "../../parse/unparse.h"

#include <print>

namespace brgen::nast::probe {

    void run_size(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        auto& t = p.tables;
        // 畳み込みが要る配列 (要素ごとに幅が違う) の数。
        each_node<ArrayType>(a, [&](Node<ArrayType> at) {
            auto* es = t.table<TypeSize>().get(at.ref(a)->element_type);
            if (es && es->kind == SizeKind::dynamic) {
                hist["  (配列: 要素が dynamic = 畳み込みが要る)"]++;
            }
        });
        each_node<Format>(a, [&](Node<Format> f) {
            auto* s = t.table<TypeSize>().get(f.ref(a)->struct_type);
            if (!s) {
                return;
            }
            const char* k = s->kind == SizeKind::fixed     ? "fixed"
                            : s->kind == SizeKind::dynamic ? "dynamic"
                                                           : "unknown";
            hist[s->kind == SizeKind::dynamic ? (s->bits_expr ? "dynamic (式あり)" : "dynamic (式なし)") : k]++;
            if (detail) {
                std::string tail;
                if (s->kind == SizeKind::fixed) {
                    tail = std::format("  {} bits ({} bytes)", s->bits, s->bits / 8);
                }
                else if (s->bits_expr) {
                    tail = "  " + unparse_node(a, s->bits_expr) + " bits";
                }
                std::println("{:<28} {}{}", name_of(a, f), k, tail);
            }
        });
    }

}  // namespace brgen::nast::probe
