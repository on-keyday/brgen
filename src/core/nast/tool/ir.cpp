/*license*/
// nast の中間表現を 1 つの .bgn について印字する。Web Playground の
// "nast-ir" はこれを wasm にしたもの (ebm の "ebm" に当たる)。
//
//   nast2ir -i <file.bgn> [--mode tree|json|unparse|unparse-self|size|endian|lower|bits]
//
//   tree    解析済みの木。ノードごとに side table (型 / 幅 / endian /
//           FormatState / Requirements ...) を併記する (nast_corpus --tree と同じ)
//   json    arena と side table をそのまま JSON で (nast_dump と同じ中身)
//   unparse 木を .bgn に戻す (parse/unparse)。解析後の木なので、parse 時に
//           畳んだ形 (config.X = v の Metadata 化など) はその形で出る
//   unparse-self  同上、解析が補った暗黙のレシーバも綴る (`len` -> `self.len`)
//   size    型の幅と幅の式            (nast_probe size)
//   endian  field ごとのバイト順      (nast_probe endian)
//   lower   lowering の形             (nast_probe lower)
//   bits    バイト境界に乗らない群    (nast_probe bits)
//
// 中身はどれも既存の道具の関数を呼ぶだけ。ここが持つのは入口だけで、
// --show-flags はバックエンドと同じ形 (backend/entry.hpp) を出すので、
// nastwebgen.py が同じ手順で Playground に載せられる。
#include "../backend/entry.hpp"
#include "../node/printer.h"
#include "../parse/unparse.h"
#include "probe/probe.hpp"

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <json/stringer.h>
#include <wrap/argv.h>
#include <wrap/cout.h>
#include <map>
#include <string>
#include <string_view>

namespace {
    using namespace brgen::nast;

    enum class Mode : std::uint8_t {
        tree,
        json,
        unparse,
        unparse_self,
        size,
        endian,
        lower,
        bits,
    };

    struct Flags : futils::cmdline::templ::HelpOption {
        std::string_view input;
        Mode mode = Mode::tree;
        bool show_flags = false;
        const char* program_name = "nast2ir";

        void bind(futils::cmdline::option::Context& ctx) {
            bind_help(ctx);
            ctx.VarString<true>(&input, "input,i", "input file (.bgn)", "FILE");
            ctx.VarMap(&mode, "mode", "what to print (default: tree)", "{tree,json,unparse,unparse-self,size,endian,lower,bits}",
                       std::map<std::string, Mode>{
                           {"tree", Mode::tree},
                           {"json", Mode::json},
                           {"unparse", Mode::unparse},
                           {"unparse-self", Mode::unparse_self},
                           {"size", Mode::size},
                           {"endian", Mode::endian},
                           {"lower", Mode::lower},
                           {"bits", Mode::bits},
                       });
            ctx.VarBool(&show_flags, "show-flags", "print what this tool is, in JSON (for tooling)");
        }
    };

    int run(Flags& flags, futils::cmdline::option::Context& ctx) {
        auto& cout = futils::wrap::cout_wrap();
        auto& cerr = futils::wrap::cerr_wrap();
        if (flags.show_flags) {
            // ir は Monaco に専用の言語が無いので plaintext で見せる
            cout << backend::entry::flag_description_json(ctx, "ir", {}, "plaintext") << '\n';
            return 0;
        }
        if (flags.input.empty()) {
            cerr << flags.program_name << ": no input (use -i FILE)\n";
            return 2;
        }
        Program p;
        if (auto r = analyze(p, flags.input); r != AnalyzeResult::ok) {
            auto why = first_error(p);
            cerr << flags.program_name << ": " << flags.input << ": " << (why.empty() ? describe(r) : why.c_str()) << '\n';
            return 1;
        }
        probe::Hist hist;
        // probe のモードは該当が無いと何も印字しないので、見出しを付けて
        // 「空」と「動かなかった」を見分けられるようにする。
        auto heading = [&](std::string_view what) {
            cout << "# " << what << " (a line per item; nothing below means none)\n";
        };
        switch (flags.mode) {
            case Mode::tree:
                cout << pretty_print(p.arena, p.tables, p.root);
                return 0;
            case Mode::json: {
                futils::json::Stringer<> s;
                {
                    auto obj_ = s.object();
                    obj_("root", p.root);
                    obj_("modules", p.modules);
                    obj_("arena", p.arena);
                    obj_("tables", p.tables);
                }
                cout << s.out() << '\n';
                return 0;
            }
            case Mode::unparse:
            case Mode::unparse_self:
                cout << brgen::nast::unparse(p.arena, p.root, UnparseOption{.explicit_self = flags.mode == Mode::unparse_self});
                return 0;
            case Mode::size:
                heading("type widths");
                probe::run_size(p, true, hist);
                return 0;
            case Mode::endian:
                heading("byte order per field");
                probe::run_endian(p, true, hist);
                return 0;
            case Mode::lower:
                heading("lowered forms");
                probe::run_lower(p, true, hist);
                return 0;
            case Mode::bits:
                heading("bit groups");
                probe::run_bits(p, true, hist);
                return 0;
        }
        return 1;
    }

    int nast_ir_main(int argc, char** argv) {
        Flags flags;
        return futils::cmdline::templ::parse_or_err<std::string>(
            argc, argv, flags,
            [&](auto&& str, bool err) {
                if (err) {
                    futils::wrap::cerr_wrap() << flags.program_name << ": " << str;
                }
                else {
                    futils::wrap::cout_wrap() << str;
                }
            },
            [&](Flags& flags, futils::cmdline::option::Context& ctx) { return run(flags, ctx); });
    }
}  // namespace

#if defined(__EMSCRIPTEN__)
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, nast_ir_main);
}
#else
int main(int argc, char** argv) {
    futils::wrap::U8Arg _(argc, argv);
    return nast_ir_main(argc, argv);
}
#endif
