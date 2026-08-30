// format ごとのビット幅を出す確認用。format 自身の StructType は「型として
// 参照された format」にしか無い (typer が遅延生成する) ので、ここでは
// FormatState.fields を TypeSize 表で積み直す。
#include "../bind/pipeline.h"
#include "../node/util.h"
#include "../parse/unparse.h"
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
        // 畳み込みが要る配列 (要素ごとに幅が違う) の数。式が書けない理由の
        // 内訳を出すため。
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (h && h->type == NodeType::ArrayType) {
                auto at = Node<ArrayType>::from_unique_id((std::uint64_t(NodeType::ArrayType) << 32) | id);
                auto* es = t.table<TypeSize>().get(at.ref(a)->element_type);
                if (es && es->kind == SizeKind::dynamic) {
                    hist["  (配列: 要素が dynamic = 畳み込みが要る)"]++;
                }
            }
            if (!h || h->type != NodeType::Format) {
                continue;
            }
            auto f = Node<Format>::from_unique_id((std::uint64_t(NodeType::Format) << 32) | id);
            auto* state = t.table<FormatState>().get(f);
            if (!state) {
                continue;
            }
            // format 自身の StructType に幅が入っている。
            auto* s = t.table<TypeSize>().get(f.ref(a)->struct_type);
            if (!s) {
                continue;
            }
            const char* k = s->kind == SizeKind::fixed ? "fixed" : s->kind == SizeKind::dynamic ? "dynamic" : "unknown";
            hist[s->kind == SizeKind::dynamic ? (s->bits_expr ? "dynamic (式あり)" : "dynamic (式なし)") : k]++;
            if (detail) {
                std::string detail_str;
                if (s->kind == SizeKind::fixed) {
                    detail_str = std::format("  {} bits ({} bytes)", s->bits, s->bits / 8);
                }
                else if (s->bits_expr) {
                    detail_str = "  " + unparse_node(a, s->bits_expr) + " bits";
                }
                std::println("{:<28} {}{}", name_of(a, f), k, detail_str);
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
