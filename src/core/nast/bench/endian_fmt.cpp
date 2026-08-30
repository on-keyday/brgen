// 動的 endian を含む format はいくつか。gate の波及元の数。
#include "../bind/pipeline.h"
#include "../node/util.h"
#include <print>
#include <set>
using namespace brgen::nast;
int main(int argc, char** argv) {
    std::size_t fmt_total = 0, fmt_dynamic = 0, fmt_nonbig = 0;
    for (int i = 1; i < argc; i++) {
        Program p;
        if (analyze(p, argv[i]) != AnalyzeResult::ok) continue;
        auto& a = p.arena;
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (!h || h->type != NodeType::Format) continue;
            auto f = Node<Format>::from_unique_id((std::uint64_t(NodeType::Format) << 32) | id);
            auto* st = p.tables.table<FormatState>().get(f);
            if (!st) continue;
            fmt_total++;
            bool dyn = false, nonbig = false;
            for (auto& fld : st->fields) {
                auto* e = p.tables.table<FieldEndian>().get(fld);
                if (!e) continue;
                if (e->dynamic) dyn = true;
                else if (e->endian != Endian::big) nonbig = true;
            }
            if (dyn) fmt_dynamic++;
            else if (nonbig) fmt_nonbig++;
        }
    }
    std::println("format {} / 動的 endian を含む {} / 非 big 固定 {}", fmt_total, fmt_dynamic, fmt_nonbig);
}
