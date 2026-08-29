// fn ごとに、本体に直接 assert/error があるか、他の fn を呼んでいるかを数える。
// 伝播を入れると何件増えるかの見当をつける。
#include "../bind/binder.hpp"
#include "../bind/import_resolver.hpp"
#include "../bind/scope_resolver.hpp"
#include "../parse/parse.h"
#include "../node/traverse.h"
#include <core/common/file.h>
#include <map>
#include <print>
#include <set>
#include <string>
#include <vector>
using namespace brgen::nast;

int main(int argc, char** argv) {
    std::size_t fns = 0, direct = 0, calls_fn = 0, both = 0;
    std::size_t reach = 0;  // 伝播後に失敗しうる fn
    for (int i = 1; i < argc; i++) {
        brgen::FileSet fs; Arena a; SideTables t; brgen::LocationError err;
        auto l = fs.add_file(std::string(argv[i])); if (!l) continue;
        auto* f = fs.get_input(*l); if (!f) continue;
        Context ctx;
        auto p = ctx.enter_stream(f, [&](Stream& s) { return parse(a, s, &err, {}); });
        if (!p) continue;
        bind::ImportResolver imp{a, t, fs, err, {}};
        imp.resolve(*p);
        bind::ScopeResolver sr{a, t, err};
        for (auto& m : imp.modules) { bind::Binder b{a, err, t}; b.bind(m); sr.resolve(m); }

        std::vector<Node<Function>> all;
        std::set<std::uint32_t> seen;
        visit_all(a, *p, [&](NodeAny n) {
            if (!seen.insert(n.id()).second) return false;
            if (auto fn = n.as_any<Function>()) all.push_back(fn);
            return true;
        });
        std::map<std::uint32_t, bool> fails;
        std::map<std::uint32_t, std::vector<std::uint32_t>> callees;
        for (auto& fn : all) {
            fns++;
            bool d = false;
            std::vector<std::uint32_t> outs;
            std::set<std::uint32_t> s2;
            visit_all(a, a.get<Function>(fn)->body, [&](NodeAny n) {
                if (!s2.insert(n.id()).second) return false;
                if (n.as_any<Assert>() || n.as_any<ExplicitError>()) d = true;
                if (auto c = n.as_any<Call>()) {
                    auto callee = a.get<Call>(c)->callee;
                    if (auto r = callee.as_any<Reference>()) {
                        if (auto* res = t.table<Resolution>().get(a.get<Reference>(r)->name)) {
                            if (auto g = res->target.as_any<Function>()) outs.push_back(g.id());
                        }
                    }
                }
                return true;
            });
            if (d) direct++;
            if (!outs.empty()) calls_fn++;
            if (d && !outs.empty()) both++;
            fails[fn.id()] = d;
            callees[fn.id()] = outs;
        }
        // 固定点
        for (bool changed = true; changed;) {
            changed = false;
            for (auto& [id, outs] : callees) {
                if (fails[id]) continue;
                for (auto o : outs) {
                    if (fails.count(o) && fails[o]) { fails[id] = true; changed = true; break; }
                }
            }
        }
        for (auto& [id, v] : fails) if (v) reach++;
    }
    std::println("fn {} 個", fns);
    std::println("  本体に直接 assert/error   {}", direct);
    std::println("  他の fn を呼ぶ            {}", calls_fn);
    std::println("  両方                      {}", both);
    std::println("  伝播後に失敗しうる        {}", reach);
}
