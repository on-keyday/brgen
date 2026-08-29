/*license*/
// 逆変換の往復検証。parse -> unparse -> 再 parse -> structural 比較 (compare.h)。
// さらに unparse をもう一度かけてテキストが動かないこと (不動点) を見る。
//
//   nast_unparse_test <file.bgn>...
//   nast_unparse_test --emit <file.bgn>     unparse 結果をそのまま出す
//
// 比較は所有木の形 (structural)。weak と位置と cosmetic は見ない。
// 失敗したファイルの unparse 結果は ignore/nast/unparse/ に書き出す。
#include "core/common/error.h"
#include "../parse/parse.h"
#include "../parse/unparse.h"
#include "../node/compare.h"

#include <core/common/file.h>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <vector>

namespace {

    void save_failed(const std::string& path, const std::string& text) {
        std::filesystem::path dir = "ignore/nast/unparse";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        auto name = std::filesystem::path(path).filename().string();
        std::ofstream ofs(dir / name, std::ios::binary);
        ofs << text;
    }

}  // namespace

namespace {

    // 由来の対応表 (spans) が本当に出力を指しているか。行と桁が範囲内で、
    // 始まりが終わりを越えていないことを見る。インデントは to_string が
    // 後から足すので、桁はインデント前の値と比べる。
    bool spans_are_sane(const brgen::nast::UnparseResult& r, std::string& why) {
        std::vector<std::size_t> width;  // 各行のインデント前の長さ
        {
            std::size_t cur = 0;
            for (char c : r.text) {
                if (c == '\n') {
                    width.push_back(cur);
                    cur = 0;
                }
                else {
                    cur++;
                }
            }
            width.push_back(cur);
        }
        for (auto& s : r.spans) {
            if (s.begin_line == 0 || s.end_line == 0) {
                why = "line number is 0-based somewhere";
                return false;
            }
            if (s.begin_line > s.end_line ||
                (s.begin_line == s.end_line && s.begin_col > s.end_col)) {
                why = "span starts after it ends";
                return false;
            }
            // to_string はインデントを足すので、行の長さはここで見る幅より
            // 長くなる。桁がインデント前の幅を越えていたら記録がずれている。
            if (s.end_line - 1 < width.size() && s.end_col > width[s.end_line - 1]) {
                why = "column runs past the end of its line";
                return false;
            }
        }
        return true;
    }

}  // namespace

int main(int argc, char** argv) {
    bool emit = false;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--emit") {
            emit = true;
        }
        else {
            paths.push_back(std::move(arg));
        }
    }
    if (paths.empty()) {
        std::println(stderr, "usage: nast_unparse_test [--emit] <file.bgn>...");
        return 2;
    }

    std::size_t ok = 0, mismatch = 0, unstable = 0, skipped = 0;
    for (auto& path : paths) {
        brgen::FileSet files;
        brgen::nast::Arena arena;
        brgen::LocationError err;
        auto loaded = files.add_file(path);
        if (!loaded) {
            skipped++;
            continue;
        }
        auto* file = files.get_input(*loaded);
        if (!file) {
            skipped++;
            continue;
        }
        brgen::nast::Context ctx;
        auto parsed = ctx.enter_stream(file, [&](brgen::nast::Stream& s) {
            return brgen::nast::parse(arena, s, &err, {});
        });
        if (!parsed) {
            // 構文エラーを含む入力。逆変換の対象外。
            skipped++;
            continue;
        }
        auto full = brgen::nast::unparse_with_spans(arena, *parsed);
        auto text = full.text;
        if (emit) {
            std::print("{}", text);
            continue;
        }
        // 同じアリーナに読み直す。structural 比較は 1 つのアリーナの中で行う。
        // add_special は本文を move で受けるのでコピーを渡す (text は後で
        // 不動点比較に使う)。
        auto reloaded = files.add_special(path + "@unparsed", std::string(text));
        if (!reloaded) {
            std::println("REPARSE-SETUP-FAIL {}", path);
            mismatch++;
            continue;
        }
        auto* file2 = files.get_input(*reloaded);
        brgen::LocationError err2;
        brgen::nast::Context ctx2;
        auto reparsed = ctx2.enter_stream(file2, [&](brgen::nast::Stream& s) {
            return brgen::nast::parse(arena, s, &err2, {});
        });
        if (!reparsed) {
            std::string msg;
            brgen::to_source_error(files)(err2).for_each_error([&](std::string_view m, bool warn) {
                if (!warn && msg.empty()) {
                    msg = m;
                }
            });
            std::println("REPARSE-FAIL {}\n{}", path, msg);
            save_failed(path, text);
            mismatch++;
            continue;
        }
        if (!brgen::nast::structural(arena, *parsed, *reparsed)) {
            std::println("MISMATCH {}", path);
            save_failed(path, text);
            mismatch++;
            continue;
        }
        std::string why;
        if (!spans_are_sane(full, why)) {
            std::println("SPAN-BROKEN {} ({})", path, why);
            save_failed(path, text);
            mismatch++;
            continue;
        }
        auto text2 = brgen::nast::unparse(arena, *reparsed);
        if (text2 != text) {
            std::println("UNSTABLE {}", path);
            save_failed(path, text + "\n=====\n" + text2);
            unstable++;
            continue;
        }
        ok++;
    }
    if (!emit) {
        std::println("\n{} ok / {} mismatch / {} unstable / {} skipped", ok, mismatch, unstable, skipped);
    }
    return (mismatch || unstable) ? 1 : 0;
}
