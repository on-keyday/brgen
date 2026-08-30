// field ごとに効いているバイト順を出す確認用。
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
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (!h || h->type != NodeType::Field) {
                continue;
            }
            auto f = Node<Field>::from_unique_id((std::uint64_t(NodeType::Field) << 32) | id);
            auto* e = p.tables.table<FieldEndian>().get(f);
            if (!e) {
                continue;
            }
            std::string k = e->dynamic ? "dynamic" : to_string(e->endian);
            hist[k]++;
            if (detail) {
                auto name = name_of(a, f);
                std::println("{:<24} {}{}", name.empty() ? "(無名)" : std::string(name), k,
                             e->dynamic ? "  " + unparse_node(a, e->dynamic.ref(a)->order) : "");
            }
        }
    }
    if (!detail) {
        for (auto& [k, v] : hist) {
            std::println("{:<10} {:>5}", k, v);
        }
    }
}
