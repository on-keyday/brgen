/*license*/
// 木を対話で見て回る入り口。中身は query/session.{hpp,cpp} にあるので、
// ここは引数を読んで Session を作るだけ。
//
//   nast_query <file.bgn>                 対話
//   nast_query <file.bgn> -c "p 12"       1 つ実行して終わり (-c は複数可)
//
// コマンドは help で出る。
//
// 出し口は futils の wrap。コンソールの符号化は UTF-8 とは限らないので、
// std::print で書くと Windows で日本語が崩れる。
#include "../query/session.hpp"

#include <wrap/cout.h>

#include <string>
#include <vector>

using namespace brgen::nast;

int main(int argc, char** argv) {
    auto& cout = futils::wrap::cout_wrap();
    auto& cerr = futils::wrap::cerr_wrap();

    std::string path;
    std::vector<std::string> once;
    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        if (arg == "-c") {
            if (i + 1 >= argc) {
                cerr << "-c にコマンドが無い\n";
                return 2;
            }
            once.push_back(argv[++i]);
            continue;
        }
        if (path.empty()) {
            path = arg;
            continue;
        }
        cerr << "余分な引数: " << arg << "\n";
        return 2;
    }
    if (path.empty()) {
        cerr << "usage: nast_query <file.bgn> [-c <command>]...\n";
        return 2;
    }

    Program p;
    auto r = analyze(p, path);
    if (r != AnalyzeResult::ok) {
        cerr << path << ": " << describe(r) << "\n";
        return 1;
    }
    // 解析は止まらないので、診断があっても中身は見られる。最初のものだけ言う。
    if (auto first = first_error(p); !first.empty()) {
        cerr << "診断あり (最初のもの): " << first << "\n";
    }

    query::Session s{p};
    if (!once.empty()) {
        for (auto& line : once) {
            std::string out;
            auto go = s.run(line, out);
            cout << out;
            if (!go) {
                break;
            }
        }
        return 0;
    }
    cout << path << " — " << std::to_string(p.arena.node_count()) << " nodes. help でコマンド一覧\n";
    query::repl(s, futils::wrap::cin_wrap(), cout);
}
