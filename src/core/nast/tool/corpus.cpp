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
#include "../bind/pipeline.h"

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

    // 段ごとの所要。「parse」と一括りにすると、ファイルを読む時間と
    // 診断を整形する時間が混ざる。分けて測る。
    struct StageTimes {
        std::chrono::nanoseconds read{};    // 開いて全部読む
        std::chrono::nanoseconds parse{};   // 字句解析 (逐次) + 構文解析
        std::chrono::nanoseconds import_{};
        std::chrono::nanoseconds bind{};
        std::chrono::nanoseconds type{};    // 型付け以降 (畳み込み / 要求 / 重ね合わせ)
        std::chrono::nanoseconds report{};  // 診断を数える / 整形する
    };

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
    StageTimes times;
    auto mark = clock::now();

    brgen::nast::AnalyzeOption aopt;
    aopt.parse = popt;
    aopt.until = parse_only ? brgen::nast::Stage::parse : brgen::nast::last_stage;
    // 段が 1 つ終わるたびに呼ばれる。前の呼び出しからの差がその段の所要。
    aopt.on_stage_done = [&](brgen::nast::Stage s) {
        auto now = clock::now();
        auto d = now - mark;
        mark = now;
        switch (s) {
            case brgen::nast::Stage::open:
                times.read += d;
                break;
            case brgen::nast::Stage::parse:
                times.parse += d;
                break;
            case brgen::nast::Stage::import_:
                times.import_ += d;
                break;
            case brgen::nast::Stage::bind:
                times.bind += d;
                break;
            default:  // type / evaluate / require / layout
                times.type += d;
                break;
        }
    };

    for (auto& path : paths) {
        brgen::nast::Program program;
        mark = clock::now();
        auto status = brgen::nast::analyze(program, path, aopt);
        auto& arena = program.arena;

        auto t_report = clock::now();
        Result r;
        r.nodes = arena.node_count();
        r.ok = status == brgen::nast::AnalyzeResult::ok;
        if (!r.ok) {
            auto msg = brgen::nast::first_error(program);
            r.message = msg.empty() ? brgen::nast::describe(status) : msg;
        }
        else {
            // error tolerant では木が返っても診断が溜まっている。件数だけ出す。
            brgen::to_source_error(program.files)(program.err).for_each_error([&](std::string_view, bool warn) {
                if (!warn) {
                    r.diagnostics++;
                }
            });
        }
        times.report += clock::now() - t_report;

        if (r.ok) {
            ok++;
            if (quiet) {
                // 何も出さない
            }
            else if (r.diagnostics) {
                std::println("ok    {:<60} {:>5} nodes  ({} diagnostics)", path, r.nodes, r.diagnostics);
            }
            else {
                auto& st = program.stats;
                std::println("ok    {:<60} {:>5} nodes  ({} resolved, {} unresolved{})",
                             path, arena.node_count(), st.names_resolved, st.names_unresolved,
                             st.imports_resolved || st.imports_failed
                                 ? std::format(", {} imports, {} import errors",
                                               st.imports_resolved, st.imports_failed)
                                 : std::string());
                auto cov = type_coverage(arena, program.modules);
                std::println("      {:<60} {:>5}/{} exprs typed, {} consts", "", cov.typed, cov.exprs, st.constants);
            }
            if (show_tree) {
                std::print("{}", brgen::nast::pretty_print(arena, program.tables, program.root, opt));
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
        auto total = times.read + times.parse + times.report + times.import_ + times.bind + times.type;
        std::println("read   {:8.1f} ms  (open and read the file)", ms(times.read));
        std::println("parse  {:8.1f} ms  (lex + parse)", ms(times.parse));
        std::println("report {:8.1f} ms  (count diagnostics)", ms(times.report));
        if (!parse_only) {
            std::println("import {:8.1f} ms", ms(times.import_));
            std::println("bind   {:8.1f} ms  (binder + scope resolver)", ms(times.bind));
            std::println("type   {:8.1f} ms", ms(times.type));
        }
        std::println("total  {:8.1f} ms", ms(total));
    }

    return ng == 0 ? 0 : 1;
}
