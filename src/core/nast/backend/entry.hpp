/*license*/
#pragma once
#include "../node/code_writer.h"
#include "../node/error.h"
#include "../node/util.h"
#include "../parse/parse.h"
#include "common.hpp"
#include "defaults.hpp"
#include "knobs.hpp"

#include "../bind/pipeline.h"

#include <core/common/file.h>
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <json/stringer.h>
#include <wrap/argv.h>
#include <wrap/cout.h>
#include <algorithm>
#include <format>
#include <fstream>
#include <functional>
#include <map>
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
        // -i の代わりに位置引数で複数渡せる。1 プロセスで回すのは、Windows では
        // 起動と DLL の読み込みが 1 回 0.8 秒ほどあり、解析本体 (0.1 秒台) より重いため。
        std::vector<std::string_view> args;
        const char* arg_desc = "[option] [FILE.bgn...]";
        // 出力を書かずに、未対応のノードを種別ごとに数えて出す。
        bool survey = false;
        // 未対応のノードに当たったときどうするか。既定は出力に目印を残す
        // (黙って消えるより、何が足りないかが出力から分かるほうがよい)。
        std::string_view unhandled = "dummy";

        const char* program_name = "";
        const char* lang_name = "";
        // 出力ファイルの既定の拡張子。LangConfig が file_extension を持っていれば入口が入れる。
        std::string_view file_extension;

        // 言語ごとの追加フラグ。LangConfig が bind(ctx) を持っていれば入口が挿す。
        // ここで登録したものは --show-flags にもそのまま出る。
        std::function<void(futils::cmdline::option::Context&)> extra_bind;

        void bind(futils::cmdline::option::Context& ctx) {
            bind_help(ctx);
            // 長いほうを先に書く。--show-flags の name はここの先頭が出るので、
            // 消費側 (ツール) が読むのは "input" であってほしい。
            ctx.VarString<true>(&input, "input,i", "input file (.bgn)", "FILE");
            ctx.VarString<true>(&output, "output,o", "output file (default: stdout; use - for stdout)", "FILE");
            ctx.VarString<true>(&unhandled, "unhandled", "what to do with an unhandled node: dummy, error, ignore", "MODE");
            ctx.VarBool(&show_flags, "show-flags", "print what this backend is, in JSON (for tooling)");
            ctx.VarBool(&survey, "survey", "write no output; count the unhandled nodes of every input by kind");
            if (extra_bind) {
                extra_bind(ctx);
            }
        }
    };

    // LangConfig の任意の要素。requires をマクロに直接書くと、型が具体的で
    // 依存名にならないぶん「無い」が false ではなく即エラーになるので、
    // ここでテンプレートを 1 枚挟む。
    template <class T>
    concept HasFileExtension = requires { T::file_extension; };

    template <class T>
    concept HasExtraFlags = requires(T& t, futils::cmdline::option::Context& c) { t.bind(c); };

    // 戻り値の型。既定は CodeWriter だが、LangConfig が result_type を宣言
    // すればそれで木を辿る。核 (Knobs / DefaultHandler / BaseContext) は元から
    // R で書いてあり、既定の実装も ON_CODEGEN() で「文字列を組む型のときだけ
    // 書く」形になっているので、CodeWriter を決め打ちしていたのは入口だけ
    // だった。
    //
    // 文字列を経由せずに組み上げたいもの (別の IR を作る、構造のまま吐く) は
    // これで書ける。条件が 2 つ:
    //   - R は既定構築できること (--unhandled ignore が空の R を返す)
    //   - --unhandled dummy は error と同じ挙動になる。目印を出力に混ぜる
    //     やり方が文字列にしか無いため。埋まっていない場所を見たいときは
    //     error で位置を出す。
    template <class T>
    concept HasResultType = requires { typename T::result_type; };

    template <class T>
    struct lang_result {
        using type = CodeWriter;
    };

    template <HasResultType T>
    struct lang_result<T> {
        using type = typename T::result_type;
    };

    template <class T>
    using lang_result_t = typename lang_result<T>::type;

    // LangConfig にあるものだけ拾う。テンプレートの中に置かないと、
    // if constexpr で切った側も実体化されてしまう。
    template <class Lang>
    void apply_lang_options(EntryFlags& flags, Lang& lang) {
        if constexpr (HasFileExtension<Lang>) {
            flags.file_extension = Lang::file_extension;
        }
        if constexpr (HasExtraFlags<Lang>) {
            flags.extra_bind = [&lang](futils::cmdline::option::Context& c) { lang.bind(c); };
        }
    }

    // 入口の裏方。brgen::nast::internal (code_writer.h) と名前で衝突するので
    // internal ではなくこの名前にしてある。
    namespace entry {

        // --show-flags の出力。手で書くと足したフラグと食い違うので、cmdline の
        // Context に登録されているものをそのまま並べる。バックエンドが自分の
        // フラグを足せば、何もしなくてもここに出る。
        //
        // rebrgen の ebmcodegen::flag_description_json に当たる。あちらにある
        // ui_lang_name / lsp_name / webworker_name / web_filtered は web 生成器
        // (ebmwebgen.py) の都合なので、対応する消費側ができるまでは置かない。
        inline auto flag_description_json(futils::cmdline::option::Context& ctx,
                                          const char* lang_name,
                                          std::string_view file_extension) {
            std::vector<std::string_view> exts;
            if (!file_extension.empty()) {
                exts.push_back(file_extension);
            }
            futils::json::Stringer<> str;
            str.set_indent("  ");
            auto root = str.object();
            root("lang_name", lang_name);
            root("file_extensions", exts);
            root("flags", [&](auto& s) {
                auto arr = s.array();
                for (auto& opt : ctx.options()) {
                    arr([&](auto& s) {
                        auto obj = s.object();
                        obj("name", opt->mainname);
                        obj("help", opt->help);
                        obj("argdesc", opt->argdesc);
                        obj("type", opt->type);
                    });
                }
            });
            root.close();
            return str.out();
        }

        template <class T>
        inline constexpr bool always_false = false;

        // 字下げ。桁の数え方も変わるので text と width は必ず一緒に決める
        // (IndentStyle::tab() / spaces(n) を使う)。
        template <class Lang>
        IndentStyle indent_style_of(Lang& lang) {
            if constexpr (requires { lang.indent_style(); }) {
                return lang.indent_style();
            }
            else {
                return IndentStyle{};
            }
        }

        // 組み上がったものを出力の中身にする。CodeWriter (と to_writer() を
        // 持つもの) はここで整形できるが、それ以外は「何を出力とするか」を
        // R を決めた側しか知らないので LangConfig::finish に投げる。
        template <class Lang, class R>
        expected<std::string> finish_output(Lang& lang, R&& r) {
            if constexpr (requires { lang.finish(std::forward<R>(r)); }) {
                return lang.finish(std::forward<R>(r));
            }
            else if constexpr (std::is_same_v<std::decay_t<R>, CodeWriter>) {
                return r.to_string(indent_style_of(lang).text.c_str());
            }
            else if constexpr (has_to_writer<std::decay_t<R>>) {
                return r.to_writer().to_string(indent_style_of(lang).text.c_str());
            }
            else {
                static_assert(always_false<Lang>,
                              "result_type is not text; give LangConfig a finish(result_type&&)");
            }
        }

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

        // 1 入力ぶんの結果。text は成功したときだけ意味がある。
        struct RunOne {
            enum class Status : std::uint8_t {
                ok,
                analyze_failed,
                backend_failed,
            } status = Status::ok;
            std::string text;
            std::vector<NodeType> unhandled;
        };

        // 解析 → knob の設定 → 木を辿る → 出力の中身にする、を 1 入力ぶん。
        // 失敗の表示はここでする (位置つきの表示に arena と files が要るため)。
        template <class R, class Lang, class Setup>
        RunOne run_one(std::string_view path, UnhandledMode mode, EntryFlags& flags, Lang& lang, Setup setup) {
            RunOne res;
            Program program;
            if (auto r = analyze(program, path); r != AnalyzeResult::ok) {
                auto why = first_error(program);
                futils::wrap::cerr_wrap()
                    << flags.program_name << ": " << path << ": "
                    << (why.empty() ? describe(r) : why.c_str()) << '\n';
                res.status = RunOne::Status::analyze_failed;
                return res;
            }
            Knobs<R> knobs;
            BaseContext<R> base{.a = program.arena, .t = program.tables, .n = knobs};
            base.config().unhandled_mode = mode;
            auto ctx = base.to_context(lang);
            auto finish = [&](LocError err) {
                report(program.arena, program.files, std::move(err));
                res.status = RunOne::Status::backend_failed;
            };
            auto collect = [&] {
                for (auto n : base.unhandled) {
                    res.unhandled.push_back(n.type());
                }
            };
            if (auto ok = setup(ctx, knobs, flags); !ok) {
                finish(ok.error());
                return res;
            }
            auto written = base.visit(program.root);
            collect();
            if (!written) {
                finish(written.error());
                return res;
            }
            auto text = finish_output(lang, std::move(*written));
            if (!text) {
                finish(text.error());
                return res;
            }
            res.text = std::move(*text);
            return res;
        }

        // --survey の集計。種別ごとに「何回出たか」と「何ファイルに出たか」。
        struct Survey {
            struct Count {
                size_t occurrences = 0;
                size_t files = 0;
            };
            std::map<std::string_view, Count> kinds;
            size_t ok = 0, analyze_failed = 0, backend_failed = 0, clean = 0;

            void add(const RunOne& r) {
                switch (r.status) {
                    case RunOne::Status::ok:
                        ok++;
                        break;
                    case RunOne::Status::analyze_failed:
                        analyze_failed++;
                        return;
                    case RunOne::Status::backend_failed:
                        backend_failed++;
                        break;
                }
                if (r.status == RunOne::Status::ok && r.unhandled.empty()) {
                    clean++;
                }
                std::map<std::string_view, size_t> here;
                for (auto t : r.unhandled) {
                    here[to_string(t)]++;
                }
                for (auto& [k, n] : here) {
                    auto& c = kinds[k];
                    c.occurrences += n;
                    c.files++;
                }
            }

            std::string render(size_t inputs) const {
                std::vector<std::pair<std::string_view, Count>> sorted(kinds.begin(), kinds.end());
                std::stable_sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) {
                    return a.second.occurrences > b.second.occurrences;
                });
                std::string out;
                out += std::format("inputs {} / ok {} (no unhandled {}) / analyze failed {} / backend failed {}\n",
                                   inputs, ok, clean, analyze_failed, backend_failed);
                out += std::format("{:>8} {:>6}  kind\n", "count", "files");
                for (auto& [k, c] : sorted) {
                    out += std::format("{:>8} {:>6}  {}\n", c.occurrences, c.files, k);
                }
                return out;
            }
        };

        template <class R, class Lang, class Setup>
        int run_inputs(EntryFlags& flags, UnhandledMode mode, Lang& lang, Setup setup) {
            std::vector<std::string_view> inputs;
            if (!flags.input.empty()) {
                inputs.push_back(flags.input);
            }
            inputs.insert(inputs.end(), flags.args.begin(), flags.args.end());
            if (inputs.empty()) {
                futils::wrap::cerr_wrap() << flags.program_name << ": no input (-i FILE or FILE...)\n";
                return 1;
            }
            if (flags.survey) {
                Survey s;
                for (auto path : inputs) {
                    s.add(run_one<R>(path, mode, flags, lang, setup));
                }
                futils::wrap::cout_wrap() << s.render(inputs.size());
                return 0;
            }
            // 書き出し先は 1 つなので、複数入力は標準出力に順に並べるだけにする。
            if (inputs.size() > 1 && !flags.output.empty() && flags.output != "-") {
                futils::wrap::cerr_wrap() << flags.program_name << ": -o takes one input; got " << inputs.size() << '\n';
                return 1;
            }
            int status = 0;
            for (auto path : inputs) {
                auto r = run_one<R>(path, mode, flags, lang, setup);
                if (r.status != RunOne::Status::ok) {
                    status = 1;
                    continue;
                }
                if (write_out(flags.output, r.text) != 0) {
                    status = 1;
                }
            }
            return status;
        }

    }  // namespace entry

}  // namespace brgen::nast::backend

// バックエンドの本体。setup は knob を設定するだけで、木を辿るのは共通側。
//
// LangConfig は `static constexpr auto lang_name` を持つ型。バックエンドが
// 状態を持ちたければそこに書く (Context::lang_config() で取れる)。
//
// 任意で置けるもの:
//   static constexpr auto file_extension = ".go";  出力の既定の拡張子
//   void bind(futils::cmdline::option::Context&)   言語ごとの追加フラグ
//                                                  (どちらも --show-flags に出る)
//   using result_type = X;                         木を辿るときの戻り値 (既定 CodeWriter)
//   std::string finish(X&&)                        X から出力を作る (X が文字列でないとき必須)
//   IndentStyle indent_style()                     字下げ (既定 4 空白, IndentStyle::tab() など)
#define NAST_BACKEND_ENTRY(LangConfig)                                                                \
    using nast_result_t = ::brgen::nast::backend::lang_result_t<LangConfig>;                          \
    using LangContext = ::brgen::nast::backend::Context<nast_result_t, LangConfig>;                   \
    static ::brgen::nast::expected<void> nast_backend_setup(                                          \
        ::brgen::nast::backend::Context<nast_result_t, LangConfig>& ctx,                              \
        ::brgen::nast::backend::Knobs<nast_result_t>& knobs,                                          \
        ::brgen::nast::backend::EntryFlags& flags);                                                   \
                                                                                                      \
    static int nast_backend_main(int argc, char** argv) {                                             \
        using namespace brgen::nast;                                                                  \
        using namespace brgen::nast::backend;                                                         \
        namespace nast_entry = ::brgen::nast::backend::entry;                                         \
        LangConfig lang;                                                                              \
        EntryFlags flags;                                                                             \
        flags.program_name = argv[0];                                                                 \
        flags.lang_name = LangConfig::lang_name;                                                      \
        apply_lang_options(flags, lang);                                                              \
        return futils::cmdline::templ::parse_or_err<std::string>(                                     \
            argc, argv, flags,                                                                        \
            [&](auto&& str, bool err) {                                                               \
                if (err) {                                                                            \
                    futils::wrap::cerr_wrap() << flags.program_name << ": " << str;                   \
                }                                                                                     \
                else {                                                                                \
                    futils::wrap::cout_wrap() << str;                                                 \
                }                                                                                     \
            },                                                                                        \
            [&](EntryFlags& flags, futils::cmdline::option::Context& opt_ctx) {                       \
                if (flags.show_flags) {                                                               \
                    futils::wrap::cout_wrap()                                                         \
                        << nast_entry::flag_description_json(                                         \
                               opt_ctx, flags.lang_name, flags.file_extension)                        \
                        << '\n';                                                                      \
                    return 0;                                                                         \
                }                                                                                     \
                auto mode = nast_entry::parse_unhandled(flags.unhandled);                             \
                if (!mode) {                                                                          \
                    futils::wrap::cerr_wrap()                                                         \
                        << flags.program_name << ": unknown --unhandled: " << flags.unhandled         \
                        << " (want dummy, error or ignore)\n";                                        \
                    return 1;                                                                         \
                }                                                                                     \
                return nast_entry::run_inputs<nast_result_t>(flags, *mode, lang, nast_backend_setup); \
            });                                                                                       \
    }                                                                                                 \
                                                                                                      \
    int main(int argc, char** argv) {                                                                 \
        futils::wrap::U8Arg _(argc, argv);                                                            \
        return nast_backend_main(argc, argv);                                                         \
    }                                                                                                 \
                                                                                                      \
    static ::brgen::nast::expected<void> nast_backend_setup(                                          \
        ::brgen::nast::backend::Context<nast_result_t, LangConfig>& ctx,                              \
        ::brgen::nast::backend::Knobs<nast_result_t>& knobs,                                          \
        ::brgen::nast::backend::EntryFlags& flags)
