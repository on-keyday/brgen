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
#include "parse.h"
#include "traverse.h"
#include "bind/binder.hpp"
#include "bind/evaluator.hpp"
#include "bind/requires.hpp"
#include "bind/import_resolver.hpp"
#include "bind/typer.hpp"
#include "bind/scope_resolver.hpp"

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

    brgen::FileSet files;
    files.set_utf_mode(brgen::UtfMode::utf8, interpret);
    brgen::expected<brgen::lexer::FileIndex, std::error_code> loaded;
    if (!path.empty()) {
        loaded = files.add_file(path);
    }
    else {
        loaded = files.add_special(stdin_name, read_stdin());
    }
    if (!loaded) {
        std::println("{{\"ok\":false,\"diagnostics\":[{{\"msg\":\"cannot open input\",\"warn\":false}}]}}");
        return 1;
    }
    auto* file = files.get_input(*loaded);
    if (!file) {
        std::println("{{\"ok\":false,\"diagnostics\":[{{\"msg\":\"cannot read input\",\"warn\":false}}]}}");
        return 1;
    }

    brgen::nast::Arena arena;
    brgen::LocationError err;
    brgen::nast::ParseOption popt;
    popt.error_tolerant = true;  // 編集途中の壊れた入力でも木を返す
    brgen::nast::Context ctx;
    brgen::nast::Node<brgen::nast::Module> root;
    auto parsed = ctx.enter_stream(file, [&](brgen::nast::Stream& s) {
        return brgen::nast::parse(arena, s, &err, popt);
    });

    std::vector<Diag> diags;
    auto collect = [&](brgen::LocationError& e) {
        for (auto& entry : e.locations) {
            diags.push_back(Diag{entry});
        }
    };
    if (!parsed) {
        collect(parsed.error());
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
    root = *parsed;

    brgen::nast::SideTables tables;
    brgen::nast::bind::ImportResolver importer{arena, tables, files, err, popt};
    brgen::nast::bind::ScopeResolver resolver{arena, tables, err};
    brgen::nast::bind::Typer typer{arena, tables, err};
    importer.resolve(root);
    for (auto& mod : importer.modules) {
        brgen::nast::bind::Binder binder{arena, err, tables};
        binder.bind(mod);
        resolver.resolve(mod);
    }
    for (auto& mod : importer.modules) {
        typer.run(mod);
    }
    brgen::nast::bind::Evaluator evaluator{arena, tables, err};
    for (auto& mod : importer.modules) {
        evaluator.run(mod);
    }
    brgen::nast::bind::RequiresInference requires_{arena, tables, typer};
    requires_.run(importer.modules);
    collect(err);

    futils::json::Stringer<> s;
    {
        auto obj_ = s.object();
        obj_("ok", true);
        obj_("main_file", std::uint64_t(*loaded));
        obj_("root", root);
        obj_("modules", importer.modules);
        obj_("diagnostics", diags);
        obj_("arena", arena);
        obj_("tables", tables);
    }
    std::println("{}", s.out());
    return 0;
}
