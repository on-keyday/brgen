/*license*/
// .bgn を nast のパーサに食わせて、構文木が組み上がるかだけを見る。
// 意味論 (スコープ構築 / 束縛 / union 導出) は parse.cpp で落としてあるので、
// ここで確認できるのは「構文の骨格が出るか」まで。
//
//   nast_corpus <file.bgn>...          1 行 1 ファイルで ok / error を出す
//   nast_corpus --tree <file.bgn>      構文木を表示する
//   nast_corpus --tree --show-null ... 埋まっていないフィールドも出す
//   nast_corpus --tree --no-weak ...   weak を落として所有辺だけにする
#include "parse.h"
#include "printer.h"

#include <core/common/file.h>
#include <print>
#include <string>
#include <vector>

namespace {

    struct Result {
        bool ok = false;
        std::size_t nodes = 0;
        std::string message;
    };

    Result run(const std::string& path, brgen::nast::Arena& arena, brgen::nast::Node<brgen::nast::Module>& root) {
        brgen::FileSet files;
        auto loaded = files.add_file(path);
        if (!loaded) {
            return {false, 0, "cannot open file"};
        }
        auto* file = files.get_input(*loaded);
        if (!file) {
            return {false, 0, "cannot read file"};
        }
        brgen::LocationError err;
        brgen::nast::Context ctx;
        auto parsed = ctx.enter_stream(file, [&](brgen::nast::Stream& s) {
            return brgen::nast::parse(arena, s, &err);
        });
        if (!parsed) {
            std::string msg;
            brgen::to_source_error(files)(parsed.error()).for_each_error([&](std::string_view m, bool warn) {
                if (!warn && msg.empty()) {
                    msg = m;
                }
            });
            return {false, arena.node_count(), msg.empty() ? "parse error" : msg};
        }
        root = *parsed;
        return {true, arena.node_count(), {}};
    }

}  // namespace

int main(int argc, char** argv) {
    bool show_tree = false;
    brgen::nast::PrintOptions opt;
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
        else {
            paths.push_back(std::move(a));
        }
    }
    if (paths.empty()) {
        std::println(stderr, "usage: nast_corpus [--tree [--show-null] [--no-weak]] <file.bgn>...");
        return 2;
    }

    std::size_t ok = 0, ng = 0;
    for (auto& path : paths) {
        brgen::nast::Arena arena;
        brgen::nast::Node<brgen::nast::Module> root;
        auto r = run(path, arena, root);
        if (r.ok) {
            ok++;
            std::println("ok    {:<60} {:>5} nodes", path, r.nodes);
            if (show_tree) {
                std::print("{}", brgen::nast::pretty_print(arena, root, opt));
            }
        }
        else {
            ng++;
            std::println("ERROR {:<60} {}", path, r.message);
        }
    }
    std::println("\n{} ok / {} error / {} total", ok, ng, ok + ng);
    return ng == 0 ? 0 : 1;
}
