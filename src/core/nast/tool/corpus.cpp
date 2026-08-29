/*license*/
// .bgn を nast のパーサに食わせて、構文木が組み上がるかだけを見る。
// 意味論 (スコープ構築 / 束縛 / union 導出) は parse.cpp で落としてあるので、
// ここで確認できるのは「構文の骨格が出るか」まで。
//
//   nast_corpus <file.bgn>...          1 行 1 ファイルで ok / error を出す
//   nast_corpus --tree <file.bgn>      構文木を表示する
//   nast_corpus --tree --show-null ... 埋まっていないフィールドも出す
//   nast_corpus --tree --no-weak ...   weak を落として所有辺だけにする
#include "core/common/error.h"
#include "../parse/parse.h"
#include "../node/printer.h"
#include "../node/traverse.h"
#include "../bind/binder.hpp"
#include "../bind/evaluator.hpp"
#include "../bind/requires.hpp"
#include "../bind/union_layout.hpp"
#include "../bind/import_resolver.hpp"
#include "../bind/typer.hpp"
#include "../bind/scope_resolver.hpp"

#include <core/common/file.h>
#include <format>
#include <chrono>
#include <print>
#include <set>
#include <string>
#include <vector>

namespace {

    // 型が付いた式の割合。typing の段が育つにつれてここが動く。
    //
    // 数えるのは木から辿れるものだけ。アリーナは解放しないので、パーサが
    // 先読みして捨てた式もそのまま残っている (コーパス全体で式の 38%)。
    // アリーナ全体を分母にすると、その分だけ薄まった数字になる。
    struct TypeCoverage {
        std::size_t exprs = 0;
        std::size_t typed = 0;
    };

    TypeCoverage type_coverage(brgen::nast::Arena& arena,
                               const std::vector<brgen::nast::Node<brgen::nast::Module>>& modules) {
        TypeCoverage c;
        // NamedArgument の name (input.align = 32 の左辺など) は指示子であって
        // 型を付ける対象ではないので、分母に入れない。
        std::set<std::uint32_t> designator;
        for (auto& mod : modules) {
            brgen::nast::visit_all(arena, mod, [&](brgen::nast::NodeAny n) {
                if (auto na = n.as_any<brgen::nast::NamedArgument>()) {
                    brgen::nast::visit_all(arena, na.ref(arena)->name,
                                           [&](brgen::nast::NodeAny d) {
                                               designator.insert(d.id());
                                               return true;
                                           });
                }
                return true;
            });
        }
        // 所有辺が 2 本入るノードがある (binder が作る StructUnionCandidate は
        // 分岐ブロックを ConditionalExpr と共有する) ので、id で重複を落とす。
        std::set<std::uint32_t> seen;
        for (auto& mod : modules) {
            brgen::nast::visit_all(arena, mod, [&](brgen::nast::NodeAny n) {
                if (!seen.insert(n.id()).second) {
                    return false;
                }
                if (designator.count(n.id())) {
                    return true;
                }
                if (auto e = n.as_any<brgen::nast::Expr>()) {
                    c.exprs++;
                    if (e.ref(arena)->type) {
                        c.typed++;
                    }
                }
                return true;
            });
        }
        return c;
    }

    struct Result {
        bool ok = false;
        std::size_t nodes = 0;
        std::string message;
        std::size_t diagnostics = 0;
    };

    // run() の中は 3 つに割れる。「parse」と一括りにすると、実際には
    // ファイルを読む時間と診断を整形する時間が混ざる。
    struct ParseTimes {
        std::chrono::nanoseconds read{};    // 開いて全部読む
        std::chrono::nanoseconds parse{};   // 字句解析 (逐次) + 構文解析
        std::chrono::nanoseconds report{};  // 診断を数える / 整形する
    };

    // files は import 解決が続けて使うので呼び出し側が持つ。
    Result run(const std::string& path, brgen::FileSet& files, brgen::nast::Arena& arena,
               brgen::nast::Node<brgen::nast::Module>& root, brgen::nast::ParseOption popt,
               ParseTimes& times) {
        using clock = std::chrono::steady_clock;
        auto t_read = clock::now();
        auto loaded = files.add_file(path);
        if (!loaded) {
            return {false, 0, "cannot open file"};
        }
        auto* file = files.get_input(*loaded);
        if (!file) {
            return {false, 0, "cannot read file"};
        }
        times.read += clock::now() - t_read;
        brgen::LocationError err;
        brgen::nast::Context ctx;
        auto t_parse = clock::now();
        auto parsed = ctx.enter_stream(file, [&](brgen::nast::Stream& s) {
            return brgen::nast::parse(arena, s, &err, popt);
        });
        times.parse += clock::now() - t_parse;
        auto t_report = clock::now();
        if (!parsed) {
            std::string msg;
            brgen::to_source_error(files)(parsed.error()).for_each_error([&](std::string_view m, bool warn) {
                if (!warn && msg.empty()) {
                    msg = m;
                }
            });
            times.report += clock::now() - t_report;
            return {false, arena.node_count(), msg.empty() ? "parse error" : msg};
        }
        root = *parsed;
        // error tolerant では木が返っても診断が溜まっている。件数だけ出す。
        std::size_t diagnostics = 0;
        brgen::to_source_error(files)(err).for_each_error([&](std::string_view, bool warn) {
            if (!warn) {
                diagnostics++;
            }
        });
        times.report += clock::now() - t_report;
        return {true, arena.node_count(), {}, diagnostics};
    }

}  // namespace

int main(int argc, char** argv) {
    bool show_tree = false;
    // 段ごとの所要時間を出す。src2json と比べるときに、木を組むところと
    // 解析するところを分けて見るためのもの。
    bool show_time = false;
    // parse だけで止める。--time と組み合わせると解析分の差が取れる。
    bool parse_only = false;
    // 1 行も出さない。計測のとき出力に時間を取られないように。
    bool quiet = false;
    brgen::nast::PrintOptions opt;
    brgen::nast::ParseOption popt;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--tree") {
            show_tree = true;
        }
        else if (a == "--show-null") {
            opt.show_null = true;
        }
        else if (a == "--no-weak") {
            opt.show_weak = false;
        }
        // 構文誤りを含む入力はこれを付けないと落ちるのが正しい (fail fast)。
        else if (a == "--error-tolerant") {
            popt.error_tolerant = true;
        }
        else if (a == "--comments") {
            popt.collect_comments = true;
        }
        else if (a == "--time") {
            show_time = true;
        }
        else if (a == "--parse-only") {
            parse_only = true;
        }
        else if (a == "--quiet") {
            quiet = true;
        }
        else {
            paths.push_back(std::move(a));
        }
    }
    if (paths.empty()) {
        std::println(stderr,
                     "usage: nast_corpus [--error-tolerant] [--comments] [--quiet] "
                     "[--time] [--parse-only] "
                     "[--tree [--show-null] [--no-weak]] <file.bgn>...");
        return 2;
    }

    std::size_t ok = 0, ng = 0;
    using clock = std::chrono::steady_clock;
    ParseTimes t_front;
    std::chrono::nanoseconds t_import{}, t_bind{}, t_type{};
    auto took = [](auto start) { return clock::now() - start; };
    for (auto& path : paths) {
        brgen::FileSet files;
        brgen::nast::Arena arena;
        brgen::nast::Node<brgen::nast::Module> root;
        auto r = run(path, files, arena, root, popt, t_front);
        brgen::nast::SideTables tables;
        brgen::LocationError err;
        // import は束縛より先。読み込んだ Module も同じ扱いで回す。
        brgen::nast::bind::ImportResolver importer{arena, tables, files, err, popt};
        brgen::nast::bind::ScopeResolver resolver{arena, tables, err};
        brgen::nast::bind::Typer typer{arena, tables, err};
        brgen::nast::bind::Evaluator evaluator{arena, tables, err};
        brgen::nast::bind::RequiresInference requires_{arena, tables, typer};
        brgen::nast::bind::UnionLayoutAnalysis union_layout{arena, tables, typer, err};
        if (!parse_only) {
            auto t1 = clock::now();
            importer.resolve(root);
            t_import += took(t1);
            auto t2 = clock::now();
            for (auto& mod : importer.modules) {
                brgen::nast::bind::Binder binder{arena, err, tables};
                binder.bind(mod);
                resolver.resolve(mod);
            }
            t_bind += took(t2);
            // 型付けは名前解決の後。Reference の型は解決先から取る。
            auto t3 = clock::now();
            for (auto& mod : importer.modules) {
                typer.run(mod);
            }
            // 定数畳み込みは型に依存しない (Resolution だけ使う) が、段としては最後。
            for (auto& mod : importer.modules) {
                evaluator.run(mod);
            }
            requires_.run(importer.modules);
            union_layout.run();
            t_type += took(t3);
        }
        if (r.ok) {
            ok++;
            if (quiet) {
                // 何も出さない
            }
            else if (r.diagnostics) {
                std::println("ok    {:<60} {:>5} nodes  ({} diagnostics)", path, r.nodes, r.diagnostics);
            }
            else {
                std::println("ok    {:<60} {:>5} nodes  ({} resolved, {} unresolved{})",
                             path, arena.node_count(), resolver.resolved, resolver.unresolved,
                             importer.resolved || importer.failed
                                 ? std::format(", {} imports, {} import errors",
                                               importer.resolved, importer.failed)
                                 : std::string());
                auto cov = type_coverage(arena, importer.modules);
                std::println("      {:<60} {:>5}/{} exprs typed, {} consts", "", cov.typed, cov.exprs, evaluator.evaluated);
            }
            if (show_tree) {
                std::print("{}", brgen::nast::pretty_print(arena, tables, root, opt));
            }
        }
        else {
            ng++;
            if (!quiet) {
                std::println("ERROR {:<60} {}", path, r.message);
            }
        }
    }
    std::println("\n{} ok / {} error / {} total", ok, ng, ok + ng);
    if (show_time) {
        auto ms = [](std::chrono::nanoseconds d) {
            return std::chrono::duration<double, std::milli>(d).count();
        };
        auto total = t_front.read + t_front.parse + t_front.report + t_import + t_bind + t_type;
        std::println("read   {:8.1f} ms  (open and read the file)", ms(t_front.read));
        std::println("parse  {:8.1f} ms  (lex + parse)", ms(t_front.parse));
        std::println("report {:8.1f} ms  (count diagnostics)", ms(t_front.report));
        if (!parse_only) {
            std::println("import {:8.1f} ms", ms(t_import));
            std::println("bind   {:8.1f} ms  (binder + scope resolver)", ms(t_bind));
            std::println("type   {:8.1f} ms", ms(t_type));
        }
        std::println("total  {:8.1f} ms", ms(total));
    }

    return ng == 0 ? 0 : 1;
}
