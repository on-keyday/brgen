// format ごとのビット幅を出す確認用。format 自身の StructType は「型として
// 参照された format」にしか無い (typer が遅延生成する) ので、ここでは
// FormatState.fields を TypeSize 表で積み直す。
#include "../bind/pipeline.h"
#include "../node/util.h"
#include <map>
#include <print>
using namespace brgen::nast;

int main(int argc, char** argv) {
    std::map<std::string, std::size_t> hist;
    bool detail = argc == 2;
    for (int i = 1; i < argc; i++) {
        Program p;
        if (analyze(p, argv[i]) != AnalyzeResult::ok) {
            continue;
        }
        auto& a = p.arena;
        auto& t = p.tables;
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (!h || h->type != NodeType::Format) {
                continue;
            }
            auto f = Node<Format>::from_unique_id((std::uint64_t(NodeType::Format) << 32) | id);
            auto* state = t.table<FormatState>().get(f);
            if (!state) {
                continue;
            }
            std::uint64_t bits = 0;
            auto kind = SizeKind::fixed;
            for (auto& fld : state->fields) {
                auto* s = t.table<TypeSize>().get(fld.ref(a)->type);
                if (!s || s->kind == SizeKind::unknown) {
                    kind = SizeKind::unknown;
                    break;
                }
                if (s->kind == SizeKind::dynamic) {
                    kind = SizeKind::dynamic;
                    continue;
                }
                bits += s->bits;
            }
            const char* k = kind == SizeKind::fixed ? "fixed" : kind == SizeKind::dynamic ? "dynamic" : "unknown";
            hist[k]++;
            if (detail) {
                std::println("{:<28} {}{}", name_of(a, f), k,
                             kind == SizeKind::fixed ? std::format("  {} bits ({} bytes)", bits, bits / 8) : "");
            }
        }
    }
    if (!detail) {
        std::size_t total = 0;
        for (auto& [k, v] : hist) {
            total += v;
        }
        for (auto& [k, v] : hist) {
            std::println("{:<10} {:>5}  {:>5.1f}%", k, v, total ? 100.0 * double(v) / double(total) : 0.0);
        }
        std::println("{:<10} {:>5}", "total", total);
    }
}
