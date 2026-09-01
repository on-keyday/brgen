/*license*/
// 木を対話で見て回る入り口。中身は query/session.{hpp,cpp} にあるので、
// ここは引数を読んで Session を作るだけ。
//
//   nast_query <file.bgn>                 対話
//   nast_query <file.bgn> -c "p 12"       1 つ実行して終わり (-c は複数可)
//
// コマンドは help で出る。
#include "../query/session.hpp"

#include <iostream>
#include <print>
#include <string>
#include <vector>

using namespace brgen::nast;

int main(int argc, char** argv) {
    std::string path;
    std::vector<std::string> once;
    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        if (arg == "-c") {
            if (i + 1 >= argc) {
                std::println(stderr, "-c にコマンドが無い");
                return 2;
            }
            once.push_back(argv[++i]);
            continue;
        }
        if (path.empty()) {
            path = arg;
            continue;
        }
        std::println(stderr, "余分な引数: {}", arg);
        return 2;
    }
    if (path.empty()) {
        std::println(stderr, "usage: nast_query <file.bgn> [-c <command>]...");
        return 2;
    }

    Program p;
    auto r = analyze(p, path);
    if (r != AnalyzeResult::ok) {
        std::println(stderr, "{}: {}", path, describe(r));
        return 1;
    }
    // 解析は止まらないので、診断があっても中身は見られる。最初のものだけ言う。
    if (auto first = first_error(p); !first.empty()) {
        std::println(stderr, "診断あり (最初のもの): {}", first);
    }

    query::Session s{p};
    if (!once.empty()) {
        for (auto& line : once) {
            std::string out;
            auto go = s.run(line, out);
            std::print("{}", out);
            if (!go) {
                break;
            }
        }
        return 0;
    }
    std::println("{} — {} nodes. help でコマンド一覧", path, p.arena.node_count());
    query::repl(s, std::cin, std::cout);
}
