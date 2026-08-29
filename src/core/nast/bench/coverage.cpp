// 式の種類ごとの数と型の付き具合、および木から到達できないノードの数。
// ファイルごとにアリーナが別なので、集合は必ずファイル単位で作り直す。
#include "../bind/binder.hpp"
#include "../bind/import_resolver.hpp"
#include "../bind/scope_resolver.hpp"
#include "../bind/typer.hpp"
#include "../parse/parse.h"
#include "../node/traverse.h"
#include <core/common/file.h>
#include <algorithm>
#include <map>
#include <print>
#include <set>
#include <string>
#include <vector>

using namespace brgen::nast;

int main(int argc, char** argv) {
    std::map<std::string, std::size_t> hist, typed_hist;
    std::map<std::string, std::size_t> ref_tgt, ref_tgt_typed;
    std::size_t total = 0, ref_unresolved = 0;
    std::size_t arena_nodes = 0, reachable_nodes = 0;
    std::size_t reach_exprs = 0, orphan_exprs = 0, designator_exprs = 0;

    for (int i = 1; i < argc; i++) {
        brgen::FileSet files;
        Arena a;
        SideTables t;
        brgen::LocationError err;
        auto loaded = files.add_file(std::string(argv[i]));
        if (!loaded) {
            continue;
        }
        auto* f = files.get_input(*loaded);
        if (!f) {
            continue;
        }
        Context ctx;
        auto parsed = ctx.enter_stream(f, [&](Stream& s) { return parse(a, s, &err, {}); });
        if (!parsed) {
            continue;
        }
        bind::ImportResolver imp{a, t, files, err, {}};
        imp.resolve(*parsed);
        bind::ScopeResolver sr{a, t, err};
        bind::Typer ty{a, t, err};
        for (auto& m : imp.modules) {
            bind::Binder b{a, err, t};
            b.bind(m);
            sr.resolve(m);
        }
        for (auto& m : imp.modules) {
            ty.run(m);
        }

        std::set<std::uint32_t> reachable;
        for (auto& m : imp.modules) {
            visit_all(a, m, [&](NodeAny n) {
                reachable.insert(n.id());
                return true;
            });
        }
        arena_nodes += a.node_count();
        reachable_nodes += reachable.size();

        // NamedArgument の name (input.align = 32 の左辺など) は指示子であって
        // 型を付ける対象ではないので、式のカバレッジの分母から外す。
        std::set<std::uint32_t> designator;
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (!h || h->type != NodeType::NamedArgument || !reachable.count(id)) {
                continue;
            }
            auto* d = a.data_at<NamedArgument>(h->data_index);
            if (!d || !d->name) {
                continue;
            }
            visit_all(a, d->name, [&](NodeAny n) {
                designator.insert(n.id());
                return true;
            });
        }

        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (!h) {
                continue;
            }
            bool live = reachable.count(id) != 0;
            visit_node_type(h->type, [&](auto tag) {
                using T = typename decltype(tag)::type;
                if constexpr (std::derived_from<T, Expr>) {
                    auto* d = a.data_at<T>(h->data_index);
                    total++;
                    if (!live) {
                        orphan_exprs++;
                        return;  // 木に無いものは分布から外す
                    }
                    if (designator.count(id)) {
                        designator_exprs++;
                        return;  // 指示子は型付けの対象外
                    }
                    reach_exprs++;
                    hist[to_string(h->type)]++;
                    if (d && d->type) {
                        typed_hist[to_string(h->type)]++;
                    }
                    if constexpr (std::is_same_v<T, Reference>) {
                        if (auto* r = t.table<Resolution>().get(d->name)) {
                            auto key = std::string(to_string(r->target.type()));
                            ref_tgt[key]++;
                            if (d->type) {
                                ref_tgt_typed[key]++;
                            }
                        }
                        else {
                            ref_unresolved++;
                        }
                    }
                }
            });
        }
    }

    std::vector<std::pair<std::string, std::size_t>> v(hist.begin(), hist.end());
    std::sort(v.begin(), v.end(), [](auto& l, auto& r) { return l.second > r.second; });
    std::size_t typed_total = 0;
    for (auto& [k, n] : v) {
        auto ty = typed_hist.count(k) ? typed_hist[k] : 0;
        typed_total += ty;
        std::println("{:<18} {:>6}  {:>5.1f}%  typed {:>6}/{:<6} {:>5.1f}%", k, n,
                     100.0 * n / reach_exprs, ty, n, 100.0 * ty / n);
    }
    std::println("");
    std::println("arena {} nodes, reachable {} ({:.1f}%)", arena_nodes, reachable_nodes,
                 100.0 * reachable_nodes / arena_nodes);
    std::println("exprs {} total, reachable {}, orphan {} ({:.1f}%), designator {} (型付け対象外)",
                 total, reach_exprs, orphan_exprs, 100.0 * orphan_exprs / total, designator_exprs);
    std::println("typed {} / {} reachable exprs ({:.1f}%)", typed_total, reach_exprs,
                 100.0 * typed_total / reach_exprs);
    std::println("");
    std::println("Reference の解決先ごと (解決エントリ無し {}):", ref_unresolved);
    std::vector<std::pair<std::string, std::size_t>> rv(ref_tgt.begin(), ref_tgt.end());
    std::sort(rv.begin(), rv.end(), [](auto& l, auto& r) { return l.second > r.second; });
    for (auto& [k, n] : rv) {
        auto ty = ref_tgt_typed.count(k) ? ref_tgt_typed[k] : 0;
        std::println("  -> {:<22} {:>6}  typed {:>6}  {:>5.1f}%", k, n, ty, 100.0 * ty / n);
    }
}
