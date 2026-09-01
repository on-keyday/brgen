/*license*/
// `nast_probe endian` — field ごとに効いているバイト順。実行時に決まるものは
// その代入 (SpecifyOrder) の式も出す。
#include "probe.hpp"
#include "../../node/util.h"
#include "../../parse/unparse.h"

#include <print>

namespace brgen::nast::probe {

    void run_endian(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        each_node<Field>(a, [&](Node<Field> f) {
            auto* e = p.tables.table<FieldEndian>().get(f);
            if (!e) {
                return;
            }
            std::string k = e->dynamic ? "dynamic" : to_string(e->endian);
            hist[k]++;
            if (detail) {
                auto name = name_of(a, f);
                std::println("{:<24} {}{}", name.empty() ? "(無名)" : std::string(name), k,
                             e->dynamic ? "  " + unparse_node(a, e->dynamic.ref(a)->order) : "");
            }
        });
        // 動的な順を含む format の数。呼び出しに乗せる規則にした場合の波及元。
        each_node<Format>(a, [&](Node<Format> f) {
            auto* st = p.tables.table<FormatState>().get(f);
            if (!st) {
                return;
            }
            for (auto& fld : st->fields) {
                auto* e = p.tables.table<FieldEndian>().get(fld);
                if (e && e->dynamic) {
                    hist["  (動的な順を含む format)"]++;
                    return;
                }
            }
        });
    }

}  // namespace brgen::nast::probe
