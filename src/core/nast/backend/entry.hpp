/*license*/
#pragma once
#include "../node/code_writer.h"
#include "../node/error.h"
#include "../parse/parse.h"
#include "common.hpp"
#include "defaults.hpp"
#include "knobs.hpp"

#include "../bind/binder.hpp"
#include "../bind/evaluator.hpp"
#include "../bind/import_resolver.hpp"
#include "../bind/requires.hpp"
#include "../bind/scope_resolver.hpp"
#include "../bind/typer.hpp"
#include "../bind/union_layout.hpp"

#include <core/common/file.h>
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/argv.h>
#include <wrap/cout.h>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

// バックエンドの入口。.bgn を読んで解析まで済ませ、knob を設定した状態で
// バックエンド本体を呼ぶところまでを引き受ける。
//
// 各バックエンドが書くのは setup 関数 1 つだけ:
//
//   struct MyLang {
//       static constexpr auto lang_name = "mylang";
//   };
//   NAST_BACKEND_ENTRY(MyLang) {
//       knobs.bind_Format(ctx, [](auto& c, Node<Format> n) { ... });
//       return {};
//   }
//
// 入力の読み込み・パイプライン・出力先・エラー表示は共通。
// 言語ごとに違うのは knob の設定と、必要なら追加のフラグだけ。
//
// rebrgen の ebmcodegen (stub/entry.hpp) に当たる。あちらとの違いは、
// 入力が .ebm ではなく .bgn なので解析の段をここで回すこと。

namespace brgen::nast::backend {

    struct EntryFlags : futils::cmdline::templ::HelpOption {
        std::string_view input;
        std::string_view output;
        bool show_flags = false;
        // 未対応のノードに当たったときどうするか。既定は出力に目印を残す
        // (黙って消えるより、何が足りないかが出力から分かるほうがよい)。
        std::string_view unhandled = "dummy";

        const char* program_name = "";
        const char* lang_name = "";
        std::string_view file_extension;

        void bind(futils::cmdline::option::Context& ctx) {
            bind_help(ctx);
            ctx.VarString<true>(&input, "i,input", "input file (.bgn)", "FILE");
            ctx.VarString<true>(&output, "o,output", "output file (default: stdout)", "FILE");
            ctx.VarString<true>(&unhandled, "unhandled", "what to do with an unhandled node: dummy, error, ignore", "MODE");
            ctx.VarBool(&show_flags, "show-flags", "print what this backend is (for tooling)");
        }
    };

    namespace internal {

        inline std::optional<UnhandledMode> parse_unhandled(std::string_view s) {
            if (s == "dummy") {
                return UnhandledMode::dummy;
            }
            if (s == "error") {
                return UnhandledMode::error;
            }
            if (s == "ignore") {
                return UnhandledMode::ignore;
            }
            return std::nullopt;
        }

        // .bgn を読んで木を組み、解析の段を全部回す。バックエンドは型や
        // 解決の結果を見て書くので、ここを飛ばすことはない。
        struct Analyzed {
            FileSet files;
            Arena arena;
            SideTables tables;
            LocationError err;
            std::vector<Node<Module>> modules;
            Node<Module> root;
        };

        inline bool analyze(Analyzed& out, std::string_view path, std::string& why) {
            auto loaded = out.files.add_file(std::string(path));
            if (!loaded) {
                why = "cannot open " + std::string(path);
                return false;
            }
            auto* file = out.files.get_input(*loaded);
            if (!file) {
                why = "cannot read " + std::string(path);
                return false;
            }
            // nast::Context (字句の入口) と backend::Context が同名なので明示する。
            ::brgen::nast::Context sctx;
            auto parsed = sctx.enter_stream(file, [&](Stream& s) {
                return parse(out.arena, s, &out.err, {});
            });
            if (!parsed) {
                to_source_error(out.files)(parsed.error()).for_each_error([&](auto& m, bool warn) {
                    if (!warn && why.empty()) {
                        why = std::string(m);
                    }
                });
                if (why.empty()) {
                    why = "parse error";
                }
                return false;
            }
            out.root = *parsed;

            bind::ImportResolver importer{out.arena, out.tables, out.files, out.err, {}};
            importer.resolve(out.root);
            bind::ScopeResolver resolver{out.arena, out.tables, out.err};
            for (auto& mod : importer.modules) {
                bind::Binder binder{out.arena, out.err, out.tables};
                binder.bind(mod);
                resolver.resolve(mod);
            }
            bind::Typer typer{out.arena, out.tables, out.err};
            for (auto& mod : importer.modules) {
                typer.run(mod);
            }
            bind::Evaluator evaluator{out.arena, out.tables, out.err};
            for (auto& mod : importer.modules) {
                evaluator.run(mod);
            }
            bind::RequiresInference requires_{out.arena, out.tables, typer};
            requires_.run(importer.modules);
            bind::UnionLayoutAnalysis layout{out.arena, out.tables, typer, out.err};
            layout.run();

            out.modules = importer.modules;
            return true;
        }

        // backend の失敗は LocError (ノードと文言) で返る。位置つきの表示に
        // するには arena を通して LocationError に直す。
        inline void report(Arena& a, FileSet& files, LocError err) {
            auto loc = err.to_location_error(a);
            to_source_error(files)(loc).for_each_error([&](auto& m, bool warn) {
                futils::wrap::cerr_wrap() << m << '\n';
            });
        }

        inline int write_out(std::string_view path, const std::string& text) {
            if (path.empty() || path == "-") {
                futils::wrap::cout_wrap() << text;
                return 0;
            }
            std::ofstream ofs(std::string(path), std::ios::binary);
            if (!ofs) {
                futils::wrap::cerr_wrap() << "cannot create " << path << '\n';
                return 1;
            }
            ofs << text;
            return 0;
        }

    }  // namespace internal

}  // namespace brgen::nast::backend

// バックエンドの本体。setup は knob を設定するだけで、木を辿るのは共通側。
//
// LangConfig は `static constexpr auto lang_name` を持つ型。バックエンドが
// 状態を持ちたければそこに書く (Context::lang_config() で取れる)。
#define NAST_BACKEND_ENTRY(LangConfig)                                                            \
    static ::brgen::nast::expected<void> nast_backend_setup(                                      \
        ::brgen::nast::backend::Context<::brgen::nast::CodeWriter, LangConfig>& ctx,              \
        ::brgen::nast::backend::Knobs<::brgen::nast::CodeWriter>& knobs,                          \
        ::brgen::nast::backend::EntryFlags& flags);                                               \
                                                                                                  \
    static int nast_backend_main(int argc, char** argv) {                                         \
        using namespace brgen::nast;                                                              \
        using namespace brgen::nast::backend;                                                     \
        EntryFlags flags;                                                                         \
        flags.program_name = argv[0];                                                             \
        flags.lang_name = LangConfig::lang_name;                                                  \
        return futils::cmdline::templ::parse_or_err<std::string>(                                 \
            argc, argv, flags,                                                                    \
            [&](auto&& str, bool err) {                                                           \
                if (err) {                                                                        \
                    futils::wrap::cerr_wrap() << flags.program_name << ": " << str;               \
                }                                                                                 \
                else {                                                                            \
                    futils::wrap::cout_wrap() << str;                                             \
                }                                                                                 \
            },                                                                                    \
            [&](EntryFlags& flags, futils::cmdline::option::Context&) {                           \
                if (flags.show_flags) {                                                           \
                    futils::wrap::cout_wrap()                                                     \
                        << "{\"lang\":\"" << flags.lang_name << "\"}\n";                          \
                    return 0;                                                                     \
                }                                                                                 \
                auto mode = ::brgen::nast::backend::internal::parse_unhandled(flags.unhandled);                           \
                if (!mode) {                                                                      \
                    futils::wrap::cerr_wrap()                                                     \
                        << flags.program_name << ": unknown --unhandled: " << flags.unhandled     \
                        << " (want dummy, error or ignore)\n";                                    \
                    return 1;                                                                     \
                }                                                                                 \
                if (flags.input.empty()) {                                                        \
                    futils::wrap::cerr_wrap() << flags.program_name << ": no input (-i FILE)\n";  \
                    return 1;                                                                     \
                }                                                                                 \
                ::brgen::nast::backend::internal::Analyzed an;                                                            \
                std::string why;                                                                  \
                if (!::brgen::nast::backend::internal::analyze(an, flags.input, why)) {                                   \
                    futils::wrap::cerr_wrap() << flags.program_name << ": " << why << '\n';       \
                    return 1;                                                                     \
                }                                                                                 \
                Knobs<CodeWriter> knobs;                                                          \
                BaseContext<CodeWriter> base{.a = an.arena, .n = knobs};                          \
                base.config().unhandled_mode = *mode;                                             \
                LangConfig lang;                                                                  \
                auto ctx = base.to_context(lang);                                                 \
                if (auto ok = nast_backend_setup(ctx, knobs, flags); !ok) {                       \
                    ::brgen::nast::backend::internal::report(an.arena, an.files, ok.error());                         \
                    return 1;                                                                     \
                }                                                                                 \
                auto written = base.visit(an.root);                                               \
                if (!written) {                                                                   \
                    ::brgen::nast::backend::internal::report(an.arena, an.files, written.error());                    \
                    return 1;                                                                     \
                }                                                                                 \
                return ::brgen::nast::backend::internal::write_out(flags.output,                                          \
                                           written->to_string(IndentStyle{}.text.c_str()));       \
            });                                                                                   \
    }                                                                                             \
                                                                                                  \
    int main(int argc, char** argv) {                                                             \
        futils::wrap::U8Arg _(argc, argv);                                                        \
        return nast_backend_main(argc, argv);                                                     \
    }                                                                                             \
                                                                                                  \
    static ::brgen::nast::expected<void> nast_backend_setup(                                      \
        ::brgen::nast::backend::Context<::brgen::nast::CodeWriter, LangConfig>& ctx,              \
        ::brgen::nast::backend::Knobs<::brgen::nast::CodeWriter>& knobs,                          \
        ::brgen::nast::backend::EntryFlags& flags)
