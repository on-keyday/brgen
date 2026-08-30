/*license*/
// ソースを 1 つ受けて、parse -> import -> bind -> type まで回した結果を
// JSON で吐く。LSP サーバー (lsp/server) が編集のたびに spawn して、
// TS 側 (nodegen.py が生成する nast_nodes.ts) がこの出力を読む。
// src2json の --print-json に当たる役で、解析はしても解釈はしない。
//
//   nast_dump <file.bgn>                       ファイルから
//   nast_dump --stdin-name <path>              stdin から (path は表示と import 解決用)
//   nast_dump --interpret-mode utf16 ...       位置を UTF-16 単位で数える
//                                              (VS Code の offsetAt に合わせる)
//
// 出力は 1 オブジェクト:
//   { "ok": bool, "main_file": n, "root": id, "modules": [id...],
//     "diagnostics": [{"msg","loc","warn"}...], "arena": {...}, "tables": {...} }
// ノード参照は unique_id (type << 32 | id)。下 32bit が 0 なら null。
#include "core/common/error.h"
#include "../parse/parse.h"
#include "../node/traverse.h"
#include "../bind/pipeline.h"

#include <core/common/file.h>
#include <json/stringer.h>
#include <cstdio>
#include <print>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

    std::string read_stdin() {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        std::string buf;
        char tmp[4096];
        std::size_t n = 0;
        while ((n = std::fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
            buf.append(tmp, n);
        }
        return buf;
    }

    struct Diag {
        brgen::LocationEntry entry;
        void as_json(auto&& s) const {
            auto obj_ = s.object();
            obj_("msg", entry.msg);
            obj_("loc", entry.loc);
            obj_("warn", entry.warn);
        }
    };

}  // namespace

int main(int argc, char** argv) {
    std::string stdin_name;
    std::string path;
    auto interpret = brgen::UtfMode::utf8;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--stdin-name" && i + 1 < argc) {
            stdin_name = argv[++i];
        }
        else if (a == "--interpret-mode" && i + 1 < argc) {
            std::string m = argv[++i];
            if (m == "utf16") {
                interpret = brgen::UtfMode::utf16;
            }
            else if (m == "utf8") {
                interpret = brgen::UtfMode::utf8;
            }
            else {
                std::println(stderr, "unknown interpret mode: {}", m);
                return 2;
            }
        }
        else if (!a.empty() && a[0] == '-') {
            std::println(stderr,
                         "usage: nast_dump [--stdin-name <path>] "
                         "[--interpret-mode utf8|utf16] [<file.bgn>]");
            return 2;
        }
        else {
            path = std::move(a);
        }
    }
    if (path.empty() && stdin_name.empty()) {
        stdin_name = "<stdin>";
    }

    brgen::nast::Program program;
    program.files.set_utf_mode(brgen::UtfMode::utf8, interpret);
    brgen::expected<brgen::lexer::FileIndex, std::error_code> loaded;
    if (!path.empty()) {
        loaded = program.files.add_file(path);
    }
    else {
        loaded = program.files.add_special(stdin_name, read_stdin());
    }
    if (!loaded) {
        std::println("{{\"ok\":false,\"diagnostics\":[{{\"msg\":\"cannot open input\",\"warn\":false}}]}}");
        return 1;
    }

    brgen::nast::AnalyzeOption aopt;
    aopt.parse.error_tolerant = true;  // 編集途中の壊れた入力でも木を返す
    auto result = brgen::nast::analyze_loaded(program, *loaded, aopt);
    if (result == brgen::nast::AnalyzeResult::cannot_read) {
        std::println("{{\"ok\":false,\"diagnostics\":[{{\"msg\":\"cannot read input\",\"warn\":false}}]}}");
        return 1;
    }

    std::vector<Diag> diags;
    for (auto& entry : program.err.locations) {
        diags.push_back(Diag{entry});
    }
    if (result != brgen::nast::AnalyzeResult::ok) {
        futils::json::Stringer<> s;
        {
            auto obj_ = s.object();
            obj_("ok", false);
            obj_("main_file", std::uint64_t(*loaded));
            obj_("diagnostics", diags);
        }
        std::println("{}", s.out());
        return 0;
    }

    auto& arena = program.arena;
    auto& tables = program.tables;

    futils::json::Stringer<> s;
    {
        auto obj_ = s.object();
        obj_("ok", true);
        obj_("main_file", std::uint64_t(*loaded));
        obj_("root", program.root);
        obj_("modules", program.modules);
        obj_("diagnostics", diags);
        obj_("arena", arena);
        obj_("tables", tables);
    }
    std::println("{}", s.out());
    return 0;
}
