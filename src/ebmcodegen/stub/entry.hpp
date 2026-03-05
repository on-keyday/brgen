/*license*/
#pragma once
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <ebm/extended_binary_module.hpp>
#include <ebmgen/debug_printer.hpp>
#include <sstream>
#include <wrap/cout.h>
#include <file/file_view.h>
#include <file/file_stream.h>
#include <json/stringer.h>
#include <set>
#include <unordered_map>
#include "ebmgen/mapping.hpp"
#include "flags.hpp"
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <tool/common/em_main.h>
#endif
#include "output.hpp"
#include <wrap/argv.h>
#include <chrono>

namespace ebmcodegen {
    struct Timepoint {
        std::chrono::steady_clock::time_point point = std::chrono::steady_clock::now();
    };

    namespace internal {
        constexpr bool is_web_type_allowed(std::string_view type_name) {
            return type_name == "file";
        }
    }  // namespace internal

    struct Flags : futils::cmdline::templ::HelpOption {
        std::string_view input;
        std::string_view output;
        bool dump_code = false;
        bool show_flags = false;
        bool timing = false;
        bool source_map = false;
        Timepoint start{};
        Timepoint prev{};
        // std::vector<std::string_view> args;
        // static constexpr auto arg_desc = "[option] args...";
        const char* program_name;
        const char* lang_name = "";
        const char* ui_lang_name = "";
        const char* lsp_name = "";
        const char* webworker_name = "";
        std::vector<std::string_view> file_extensions;

        bool debug_unimplemented = false;

        std::string_view dump_test_file;
        std::string_view dump_test_separator;

        std::set<std::string_view> web_filtered;
        std::unordered_map<std::string_view, std::string_view> web_type_map;

        std::unordered_map<std::string_view, std::string_view*> config_map;

        std::string_view get_config(std::string_view name) {
            auto it = config_map.find(name);
            if (it != config_map.end()) {
                return *(it->second);
            }
            return "";
        }

        void debug_timing(const char* phase) {
            if (timing) {
                auto now = Timepoint{};
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now.point - prev.point).count();
                auto from_start = std::chrono::duration_cast<std::chrono::milliseconds>(now.point - start.point).count();
                futils::wrap::cerr_wrap() << program_name << ": [timing] " << phase << " took " << diff << " ms (total " << from_start << " ms)\n";
                prev = now;
            }
        }

        void bind(futils::cmdline::option::Context& ctx) {
            bind_help(ctx);
            ctx.VarString<true>(&input, "input,i", "input EBM file", "FILE");
            ctx.VarString<true>(&output, "output,o", "output source code file (currently not working and always output to stdout)", "FILE");
            ctx.VarBool(&show_flags, "show-flags", "show all flags (for debug and code generation)");
            ctx.VarBool(&dump_code, "dump-code", "dump code (for debug)");
            ctx.VarString<true>(&dump_test_file, "test-info", "dump test info file", "FILE");
            ctx.VarString<true>(&dump_test_separator, "test-separator", "dump test info separator when dumping test info to stdout", "SEP");
            ctx.VarBool(&debug_unimplemented, "debug-unimplemented", "debug unimplemented node (for debug)");
            ctx.VarBool(&timing, "timing", "show timing info (for debug)");
            ctx.VarBoolFunc(&source_map, "source-map", "Generates WebPlayground/API Server compatible source-map output (same as --test-info - --test-separator \"############\")", [&](bool flag, auto) {
                if (flag) {
                    dump_test_file = "-";
                    dump_test_separator = "############";
                    source_map = true;
                }
                else {
                    dump_test_file = "";
                    dump_test_separator = "";
                    source_map = false;
                }
                return true;
            });
            web_filtered = {"help", "input", "output", "show-flags", "dump-code", "test-info", "test-separator", "timing"};
        }
    };
    namespace internal {
        int load_file(auto& flags, auto& output, futils::cmdline::option::Context& ctx, auto&& then) {
            if (flags.show_flags) {
                futils::wrap::cout_wrap() << flag_description_json(ctx, flags.lang_name, flags.ui_lang_name, flags.lsp_name, flags.webworker_name, flags.file_extensions, flags.web_filtered, flags.web_type_map) << '\n';
                return 0;
            }
            if (flags.input.empty()) {
                futils::wrap::cerr_wrap() << flags.program_name << ": " << "no input file\n";
                return 1;
            }
            ebm::ExtendedBinaryModule ebm;
            auto& cout = futils::wrap::cout_wrap();
            auto& cerr = futils::wrap::cerr_wrap();
            flags.debug_timing("start loading file");
            futils::file::View view;
            if (auto res = view.open(flags.input); !res) {
                cerr << flags.program_name << ": " << res.error().template error<std::string>() << '\n';
                return 1;
            }
            if (!view.data()) {
                cerr << flags.program_name << ": " << "Empty file\n";
                return 1;
            }
            flags.debug_timing("file opened");
            futils::binary::reader r{view};
            auto err = ebm.decode(r);
            flags.debug_timing("file decoded");
            if (err) {
                if (flags.dump_code) {
                    std::stringstream ss;
                    ebmgen::MappingTable table(ebm);
                    ebmgen::DebugPrinter printer(table, ss);
                    printer.print_module();
                    cout << ss.str();
                }
                cerr << flags.program_name << ": " << err.error<std::string>() << '\n';
                return 1;
            }
            if (!r.empty()) {
                if (flags.dump_code) {
                    std::stringstream ss;
                    ebmgen::MappingTable table(ebm);
                    ebmgen::DebugPrinter printer(table, ss);
                    printer.print_module();
                    cout << ss.str();
                }
                cerr << flags.program_name << ": " << "unexpected remaining data for input\n";
                return 1;
            }
            futils::file::FileStream<std::string> fs{futils::file::File::stdout_file()};
            futils::binary::writer w{fs.get_direct_write_handler(), &fs};
            flags.debug_timing("file loaded");
            int ret = then(w, ebm, output);
            if (flags.dump_test_file.size()) {
                futils::json::Stringer str;
                auto obj = str.object();
                obj("line_map", output.line_maps);
                obj("structs", output.struct_names);
                obj.close();
                if (flags.dump_test_file == "-") {
                    cout << flags.dump_test_separator;
                    cout << str.out() << "\n";
                    return ret;
                }
                auto file = futils::file::File::create(flags.dump_test_file);
                if (!file) {
                    cerr << flags.program_name << ": " << file.error().template error<std::string>() << '\n';
                    return 1;
                }
                futils::file::FileStream<std::string> fs{*file};
                futils::binary::writer w{fs.get_direct_write_handler(), &fs};
                w.write(str.out());
            }
            return ret;
        }
    }  // namespace internal
}  // namespace ebmcodegen

#define DEFINE_ENTRY(FlagType, OutputType)                                                                                                                                                                                    \
    int Main(FlagType& flags, futils::cmdline::option::Context& ctx, futils::binary::writer& w, ebm::ExtendedBinaryModule& ebm, OutputType& output);                                                                          \
    int ebmcodegen_main(int argc, char** argv) {                                                                                                                                                                              \
        FlagType flags;                                                                                                                                                                                                       \
        OutputType output;                                                                                                                                                                                                    \
        flags.program_name = argv[0];                                                                                                                                                                                         \
        return futils::cmdline::templ::parse_or_err<std::string>(                                                                                                                                                             \
            argc, argv, flags, [&](auto&& str, bool err) {  if(err){ futils::wrap::cerr_wrap()<< flags.program_name << ": " <<str; } else { futils::wrap::cout_wrap() << str;} },                                                                                                                                                                 \
            [&](FlagType& flags, futils::cmdline::option::Context& ctx) { return ebmcodegen::internal::load_file(flags, output, ctx, [&](auto& w, auto& ebm, auto& output) { return Main(flags, ctx, w, ebm, output); }); }); \
    }                                                                                                                                                                                                                         \
    int Main(FlagType& flags, futils::cmdline::option::Context& ctx, futils::binary::writer& w, ebm::ExtendedBinaryModule& ebm, OutputType& output)

int ebmcodegen_main(int argc, char** argv);
#if defined(__EMSCRIPTEN__)
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, ebmcodegen_main);
}
#else
int main(int argc, char** argv) {
    futils::wrap::U8Arg _(argc, argv);
    return ebmcodegen_main(argc, argv);
}
#endif
