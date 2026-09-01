/*license*/
// 入り口。見たいものを最初の引数で選ぶ。中身はモードごとに size.cpp /
// endian.cpp / lower.cpp にある。
#include "../../node/console.h"
#include "probe.hpp"

#include <string_view>

using namespace brgen::nast;
using namespace brgen::nast::probe;

int main(int argc, char** argv) {
    if (argc < 3) {
        print_line(stderr, "usage: nast_probe <size|endian|lower> <file.bgn>...");
        print_line(stderr, "  ファイルが 1 つなら明細、2 つ以上なら集計");
        return 2;
    }
    std::string_view what = argv[1];
    bool detail = argc == 3;
    Hist hist;
    std::size_t files = 0;
    for (int i = 2; i < argc; i++) {
        Program p;
        if (analyze(p, argv[i]) != AnalyzeResult::ok) {
            continue;
        }
        files++;
        if (what == "size") {
            run_size(p, detail, hist);
        }
        else if (what == "endian") {
            run_endian(p, detail, hist);
        }
        else if (what == "lower") {
            run_lower(p, detail, hist);
        }
        else {
            print_line(stderr, "unknown mode: {}", what);
            return 2;
        }
    }
    if (!detail) {
        // 行は母集団ごとに分かれている (`field: ...` と lowering の規則)。
        // 混ぜて割ると割合が意味を失うので、`:` の手前を群として分けて数える。
        // 先頭が空白の行は内訳なので母数に入れない。
        std::map<std::string, std::size_t> group_total;
        auto group_of = [](const std::string& k) {
            auto pos = k.find(':');
            return pos == std::string::npos ? std::string() : k.substr(0, pos);
        };
        for (auto& [k, v] : hist) {
            if (!k.starts_with("  ")) {
                group_total[group_of(k)] += v;
            }
        }
        for (auto& [k, v] : hist) {
            auto total = k.starts_with("  ") ? 0 : group_total[group_of(k)];
            print_line("{:<32} {:>6}{}", k, v,
                         total ? std::format("  {:>5.1f}%", 100.0 * double(v) / double(total)) : "");
        }
        for (auto& [g, v] : group_total) {
            print_line("{:<32} {:>6}", g.empty() ? "合計" : g + " 合計", v);
        }
        print_line("({} files)", files);
    }
}
