/*license*/
#pragma once
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <bm/binary_module.hpp>
#include <wrap/cout.h>
#include <file/file_view.h>
#include <file/file_stream.h>
#include <json/stringer.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <tool/common/em_main.h>
#endif
#include "output.hpp"

namespace bm2 {
    struct Flags : futils::cmdline::templ::HelpOption {
        std::string_view input;
        std::string_view output;
        bool dump_code = false;
        // std::vector<std::string_view> args;
        // static constexpr auto arg_desc = "[option] args...";
        const char* program_name;

        std::string_view dump_test_file;

        void bind(futils::cmdline::option::Context& ctx) {
            bind_help(ctx);
            ctx.VarString<true>(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
            ctx.VarString<true>(&output, "o,output", "output file", "FILE");
            ctx.VarBool(&dump_code, "dump-code", "dump code (for debug)");
            ctx.VarString<true>(&dump_test_file, "test-info", "dump test info file", "FILE");
        }
    };
    namespace internal {
        int load_file(auto& flags, auto& output, futils::cmdline::option::Context& ctx, auto&& then) {
            rebgn::BinaryModule bm;
            auto& cout = futils::wrap::cout_wrap();
            auto& cerr = futils::wrap::cerr_wrap();
            futils::file::View view;
            if (auto res = view.open(flags.input); !res) {
                cerr << flags.program_name << ": " << res.error().template error<std::string>() << '\n';
                return 1;
            }
            if (!view.data()) {
                cerr << flags.program_name << ": " << "Empty file\n";
                return 1;
            }
            futils::binary::reader r{view};
            auto err = bm.decode(r);
            if (err) {
                if (flags.dump_code) {
                    for (auto& code : bm.code) {
                        cout << to_string(code.op) << '\n';
                    }
                }
                cerr << flags.program_name << ": " << err.error<std::string>() << '\n';
                return 1;
            }
            futils::file::FileStream<std::string> fs{futils::file::File::stdout_file()};
            futils::binary::writer w{fs.get_direct_write_handler(), &fs};
            int ret = then(w, bm, output);
            if (flags.dump_test_file.size()) {
                auto file = futils::file::File::create(flags.dump_test_file);
                if (!file) {
                    cerr << flags.program_name << ": " << file.error().template error<std::string>() << '\n';
                    return 1;
                }
                futils::file::FileStream<std::string> fs{*file};
                futils::binary::writer w{fs.get_direct_write_handler(), &fs};
                futils::json::Stringer str;
                auto obj = str.object();
                obj("line_map", [&](auto& s) { auto _ = s.array(); });  // for future use
                obj("structs", output.struct_names);
                obj.close();
                w.write(str.out());
            }
            return ret;
        }
    }  // namespace internal
}  // namespace bm2

#define DEFINE_ENTRY(FlagType, OutputType)                                                                                                                                                                           \
    int Main(FlagType& flags, futils::cmdline::option::Context& ctx, futils::binary::writer& w, rebgn::BinaryModule& bm, OutputType& output);                                                                        \
    int bm2_main(int argc, char** argv) {                                                                                                                                                                            \
        FlagType flags;                                                                                                                                                                                              \
        OutputType output;                                                                                                                                                                                           \
        flags.program_name = argv[0];                                                                                                                                                                                \
        return futils::cmdline::templ::parse_or_err<std::string>(                                                                                                                                                    \
            argc, argv, flags, [&](auto&& str, bool err) {  if(err){ futils::wrap::cerr_wrap()<< flags.program_name << ": " <<str; } else { futils::wrap::cout_wrap() << str;} },                                                                                                                                                        \
            [&](FlagType& flags, futils::cmdline::option::Context& ctx) { return bm2::internal::load_file(flags, output, ctx, [&](auto& w, auto& bm, auto& output) { return Main(flags, ctx, w, bm, output); }); }); \
    }                                                                                                                                                                                                                \
    int Main(FlagType& flags, futils::cmdline::option::Context& ctx, futils::binary::writer& w, rebgn::BinaryModule& bm, OutputType& output)

int bm2_main(int argc, char** argv);
#if defined(__EMSCRIPTEN__)
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, bm2_main);
}
#else
int main(int argc, char** argv) {
    return bm2_main(argc, argv);
}
#endif
