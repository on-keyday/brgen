/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "binary/discard.h"
#include "common.hpp"
#include "core/ast/file.h"
#include "core/byte.h"
#include <wrap/iostream.h>
#include "ebmcodegen/stub/flags.hpp"
#include "ebmgen/json_printer.hpp"
#include "ebmgen/mapping.hpp"
#include "file/file.h"
#include "file/file_view.h"
#include "fnet/util/base64.h"
#include "json/stringer.h"
#include "load_json.hpp"
#include "convert.hpp"
#include "debug_printer.hpp"  // Include the new header
#include "stdin.hpp"
#include "transform/control_flow_graph.hpp"
#include "unicode/utf/convert.h"
#include "wrap/argv.h"
#include "wrap/cin.h"
#include "wrap/light/string.h"
#include <file/file_stream.h>  // Required for futils::file::FileStream
#include <binary/writer.h>     // Required for futils::binary::writer
#include <fstream>             // Required for std::ofstream
#include <optional>
#include <sstream>  // Required for std::stringstream
#include <platform/lazy_dll.h>
#include <tool/src2json/capi.h>  // libs2j C API
#include <env/env_sys.h>
#include <wrap/exepath.h>
#include <filesystem>
#include "interactive/debugger.hpp"
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <testutil/timer.h>
#include <number/hex/bin2hex.h>

enum class DebugOutputFormat {
    Text,
    JSON,
};

enum class OutputFormat {
    Binary,
    Base64,
    Hex,
};

enum class InputFormat {
    AUTO,
    BGN,  // with libs2j
    JSON_AST,
    JSON_EBM,  // direct EBM JSON format, for testing
    EBM,
};

enum class QueryOutputFormat {
    ID,
    Text,
    JSON,
    Hex,
};

struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view input;
    std::string_view output;
    std::string_view debug_output;  // New flag for debug output
    InputFormat input_format = InputFormat::AUTO;
    DebugOutputFormat debug_format = DebugOutputFormat::Text;
    QueryOutputFormat query_output_format = QueryOutputFormat::ID;
    std::string_view cfg_output;
    OutputFormat output_format = OutputFormat::Binary;
    bool verbose = false;
    bool debug = false;
    std::string_view libs2j_path;  // Path to libs2j directory
    std::string env_libs2j_path;
    bool interactive = false;
    bool show_flags = false;
    std::string_view query;
    bool timing = false;
    bool print_output_size = false;
    bool verify_uniqueness = false;

    void bind(futils::cmdline::option::Context& ctx) {
        auto exe_path = futils::wrap::get_exepath();
        auto exe_dir = std::filesystem::path(exe_path).parent_path();
        auto joined = (exe_dir / "libs2j" futils_default_dll_suffix).generic_u8string();
        env_libs2j_path = futils::env::sys::env_getter().get_or<std::string>("LIBS2J_PATH", futils::strutil::concat<std::string>(joined));
        libs2j_path = env_libs2j_path;
        bind_help(ctx);
        ctx.VarString<true>(&input, "input,i", "input file", "FILE");
        ctx.VarMap(&input_format, "input-format", "input format (default: decided by file extension)", "{json-ast,ebm,bgn,json-ebm}",
                   std::map<std::string, InputFormat>{
                       {"bgn", InputFormat::BGN},
                       {"ebm", InputFormat::EBM},
                       {"json-ast", InputFormat::JSON_AST},
                       {"json-ebm", InputFormat::JSON_EBM},
                   });
        ctx.VarString<true>(&output, "output,o", "output file (if -, write to stdout)", "FILE");
        ctx.VarString<true>(&debug_output, "debug-print,d", "debug output file (if -, write to stdout)", "FILE");
        ctx.VarMap(&debug_format, "debug-format", "debug output format (default: text)", "{text,json}",
                   std::map<std::string, DebugOutputFormat>{
                       {"text", DebugOutputFormat::Text},
                       {"json", DebugOutputFormat::JSON},
                   });
        ctx.VarMap(&query_output_format, "query-format", "query output format (default: id)", "{id,text,json}",
                   std::map<std::string, QueryOutputFormat>{
                       {"id", QueryOutputFormat::ID},
                       {"text", QueryOutputFormat::Text},
                       {"json", QueryOutputFormat::JSON},
                       {"hex", QueryOutputFormat::Hex},

                   });
        ctx.VarString<true>(&cfg_output, "cfg-output,c", "control flow graph output file (if -, write to stdout)", "FILE");
        ctx.VarBoolFunc(&output_format, "base64", "output as base64 encoding (for web playground compatibility)", [&](bool y, auto) {
            if (y) {
                output_format = OutputFormat::Base64;
            }
            return true;
        });
        ctx.VarMap(&output_format, "output-format", "output format (default: binary)", "{binary,base64,hex}",
                   std::map<std::string, OutputFormat>{
                       {"binary", OutputFormat::Binary},
                       {"base64", OutputFormat::Base64},
                       {"hex", OutputFormat::Hex},
                   });
        ctx.VarBool(&verbose, "verbose,v", "verbose output (for debug)");
        ctx.VarBool(&debug, "debug,g", "enable debug transformations (do not remove unused items)");
        ctx.VarString<true>(&libs2j_path, "libs2j-path", "path to libs2j (default: {executable_dir}/libs2j" futils_default_dll_suffix ")", "PATH");
        ctx.VarBool(&interactive, "interactive,I", "start interactive debugger");
        ctx.VarBool(&show_flags, "show-flags", "output command line flag description in JSON format");
        ctx.VarString<true>(&query, "query,q", "run query to object and output matched objects to stdout", "QUERY");
        ctx.VarBool(&timing, "timing", "Processing timing (for performance debug)");
        ctx.VarBool(&print_output_size, "output-size", "print output size to stderr (for debugging)");
        ctx.VarBool(&verify_uniqueness, "verify-uniqueness", "verify uniqueness of identifiers during conversion (for debugging)");
    }
};

auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();

#define TIMING(text)                                                  \
    if (flags.timing) {                                               \
        cerr << std::format("Timing: {}: {}\n", text, t.next_step()); \
    }

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.show_flags) {
        cout << ebmcodegen::flag_description_json(ctx, "ebm", "ebm", "text", "ebmgen", {".ebm", ".ebm.json", ".txt"}, std::unordered_set<std::string>{"help", "show-flags"}, std::unordered_map<std::string_view, std::string_view>{});
        return 0;
    }
    if (flags.input.empty()) {
        cerr << "error: input file is required\n";
        return 1;
    }
    ebmgen::Stdin stdin_data;
    if (flags.input == "-") {
        if (flags.interactive) {
            cerr << "error: interactive mode is not supported for stdin input\n";
            return 1;
        }
        if (flags.input_format == InputFormat::AUTO) {
            cerr << "error: cannot use auto input format detection for stdin input. please specify --input-format\n";
            return 1;
        }
        auto stdin_result = stdin_data.try_read_stdin();
        if (!stdin_result) {
            cerr << "error: failed to read stdin: " << stdin_result.error().error<std::string>() << '\n';
            return 1;
        }
    }
    ebmgen::verbose_error = flags.verbose;
    if (flags.input_format == InputFormat::AUTO) {
        if (flags.input.ends_with(".bgn")) {
            flags.input_format = InputFormat::BGN;
        }
        else if (flags.input.ends_with(".ebm")) {
            flags.input_format = InputFormat::EBM;
        }
        else if (flags.input.ends_with(".ebm.json")) {
            flags.input_format = InputFormat::JSON_EBM;
        }
        else if (flags.input.ends_with(".json")) {
            flags.input_format = InputFormat::JSON_AST;
        }
        else {
            cerr << "Cannot detect input format from file extension. Please specify --input-format\n";
            return 1;
        }
    }
    futils::test::Timer t;
    ebm::ExtendedBinaryModule ebm;
    std::optional<ebmgen::Output> out;
    if (flags.input_format == InputFormat::EBM) {
        futils::binary::reader r{futils::view::rvec{}};
        futils::error::Error<> err;
        if (stdin_data.stdin_data) {
            r.reset_buffer(*stdin_data.stdin_data);
            err = ebm.decode(r);
        }
        else {
            futils::file::View view;
            if (auto res = view.open(flags.input); !res) {
                cerr << "error: failed to open " << flags.input << ": " << res.error().template error<std::string>() << '\n';
                return 1;
            }
            if (!view.data()) {
                cerr << "error: " << "Empty file\n";
                return 1;
            }
            r.reset_buffer(futils::view::rvec(view));
            err = ebm.decode(r);
        }
        if (err) {
            cerr << "error: failed to load ebm: " << err.template error<std::string>() << '\n';
            return 1;
        }
        if (!r.empty()) {
            cerr << "error: extra data at the end of file\n";
            return 1;
        }
        TIMING("load");
    }
    else if (flags.input_format == InputFormat::JSON_EBM) {
        ebmgen::expected<ebm::ExtendedBinaryModule> ret;
        if (stdin_data.stdin_data) {
            ret = ebmgen::decode_json_ebm(*stdin_data.stdin_data);
        }
        else {
            ret = ebmgen::load_json_ebm(flags.input);
        }
        if (!ret) {
            cerr << "Load Error: " << ret.error().error<std::string>() << '\n';
            return 1;
        }
        ebm = std::move(*ret);
        TIMING("load");
    }
    else {
        futils::wrap::path_string path = futils::utf::convert<futils::wrap::path_string>(flags.libs2j_path);
        futils::platform::dll::DLL libs2j(path.c_str(), false);  // this is lazy load, so if not need, not loaded
        futils::platform::dll::Func<decltype(libs2j_call)> libs2j_call(libs2j, "libs2j_call");
        ebmgen::expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> ast;  // NOTE: definition order of this `ast` definition is important for `direct ast pass` destructor execution
        if (flags.input_format == InputFormat::BGN) {
            auto input = flags.input.data();
            int argc = 5;
            CAPABILITY capabilities = S2J_CAPABILITY_FILE | S2J_CAPABILITY_IMPORTER | S2J_CAPABILITY_PARSER | S2J_CAPABILITY_AST_JSON | S2J_CAPABILITY_DIRECT_AST_PASS;
            const char* argv[] = {"libs2j", "--no-color", "--print-json", "--print-on-error", input, nullptr, nullptr, nullptr, nullptr};
            std::string argv_size;
            if (stdin_data.stdin_data) {
                argv[4] = "--sized-argv";
                argv[5] = (const char*)stdin_data.stdin_data->data();
                futils::number::to_string(argv_size, stdin_data.stdin_data->size());
                argv[6] = "--sized-argv-size";
                argv[7] = argv_size.c_str();
                capabilities |= S2J_CAPABILITY_ARGV;
                argc = 8;
            }
            auto callback = [](const char* data, size_t len, size_t is_error, void* ast_raw) {
                decltype(ast)* astp = (decltype(ast)*)ast_raw;
                if (IS_DIRECT_AST_PASS(is_error)) {
                    if (sizeof(brgen::ast::DirectASTPassInterface) != len) {
                        *astp = ebmgen::unexpect_error("size of DirectASTPassInterface mismatch");
                        return;
                    }
                    brgen::ast::DirectASTPassInterface* p = (brgen::ast::DirectASTPassInterface*)data;
                    *astp = {*p->ast, *p->files};
                    return;
                }
                if (IS_STDERR(is_error)) {
                    cerr << std::string_view(data, len);
                    return;
                }
                *astp = ebmgen::load_json_file(std::string_view(data, len), nullptr);
            };
            if (!libs2j_call.find()) {  // load dll here
                cerr << "Failed to load libs2j_call from " << flags.libs2j_path << '\n';
                return 1;
            }
            int ret = libs2j_call(argc, (char**)argv, capabilities, +callback, &ast);
            if (ret != 0) {
                cerr << "libs2j failed: " << ret << '\n';
                return 1;
            }
        }
        else {
            if (stdin_data.stdin_data) {
                ast = ebmgen::load_json_file(*stdin_data.stdin_data, nullptr);
            }
            else {
                ast = ebmgen::load_json(flags.input, nullptr);
            }
        }

        if (!ast) {
            cerr << ast.error().error<std::string>() << '\n';
            return 1;
        }
        TIMING("load and parse");

        auto output = ebmgen::convert_ast_to_ebm(ast->first, std::move(ast->second), ebm, {.not_remove_unused = flags.debug, .verify_uniqueness = flags.verify_uniqueness, .timer_cb = [&](const char* phase) {
                                                                                               TIMING(phase);
                                                                                           }});
        if (!output) {
            cerr << "Convert Error: " << output.error().error<std::string>() << '\n';
            return 1;
        }
        out = std::move(*output);
    }

    std::optional<ebmgen::MappingTable> table;
    if (!flags.debug_output.empty() || !flags.cfg_output.empty() || flags.interactive || !flags.query.empty()) {
        table.emplace(ebm);
    }
    if (flags.input_format == InputFormat::EBM || flags.input_format == InputFormat::JSON_EBM) {
        if (!table) {
            table.emplace(ebm);
        }
        if (!table->valid()) {
            cerr << "error: invalid ebm structure; " << table->original_id_count() << " original ids, " << table->mapped_id_count() << " mapped ids\n";
            return 1;
        }
    }

    // Debug print if requested
    if (!flags.debug_output.empty()) {
        auto save_to_file = [&](auto&& out) {
            if (flags.debug_output == "-") {
                cout << out;
            }
            else {
                std::ofstream debug_ofs(std::string(flags.debug_output));
                if (!debug_ofs.is_open()) {
                    cerr << "Failed to open debug output file: " << flags.debug_output << '\n';
                    return 1;
                }
                debug_ofs << out;
                debug_ofs.close();
            }
            return 0;
        };
        if (flags.debug_format == DebugOutputFormat::Text) {
            std::stringstream debug_ss;
            ebmgen::DebugPrinter printer(*table, debug_ss);

            printer.print_module();
            auto ret = save_to_file(debug_ss.str());
            if (ret != 0) {
                return ret;
            }
        }
        else if (flags.debug_format == DebugOutputFormat::JSON) {
            ebmgen::JSONPrinter p(*table);
            futils::json::Stringer<> s;
            p.print_module(s);
            auto ret = save_to_file(s.out());
            if (ret != 0) {
                return ret;
            }
        }
        else {
            cerr << "Unsupported format\n";
            return 1;
        }
        TIMING("debug output");
    }

    // also generates control flow graph
    if (!flags.cfg_output.empty()) {
        ebmgen::CFGStack stack;
        auto cfgs = ebmgen::analyze_control_flow_graph(stack, {&*table, &ebm.statements});
        if (!cfgs) {
            cerr << "Failed to analyze control flow graph: " << cfgs.error().error<std::string>() << '\n';
            return 1;
        }
        TIMING("cfg generate");
        if (flags.cfg_output == "-") {
            std::string buffer;
            futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
            ebmgen::write_cfg(w, *cfgs, *table);
            cout << buffer;
        }
        else {
            std::ofstream debug_ofs(std::string(flags.cfg_output));
            if (!debug_ofs.is_open()) {
                cerr << "Failed to open control flow graph file: " << flags.cfg_output << '\n';
                return 1;
            }
            futils::binary::writer w{&futils::wrap::iostream_adapter<futils::byte>::out, &debug_ofs};
            ebmgen::write_cfg(w, *cfgs, *table);
        }
        TIMING("cfg output");
    }

    auto write_ebm = [&](futils::binary::writer& w) {
        if (flags.output_format == OutputFormat::Hex) {
            std::string buffer;
            futils::binary::writer temp_w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
            auto err = ebm.encode(temp_w);
            if (err) {
                cerr << "Failed to encode EBM: " << err.error<std::string>() << '\n';
                return 1;
            }
            std::string hex_output;
            futils::number::hex::to_hex(hex_output, buffer);
            if (!w.write(hex_output)) {
                cerr << "Failed to write hex output\n";
                return 1;
            }
        }
        else if (flags.output_format == OutputFormat::Base64) {
            std::string buffer;
            futils::binary::writer temp_w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
            auto err = ebm.encode(temp_w);
            if (err) {
                cerr << "Failed to encode EBM: " << err.error<std::string>() << '\n';
                return 1;
            }
            std::string output;
            if (!futils::base64::encode(buffer, output)) {
                cerr << "Failed to encode EBM to base64: MAYBE BUG\n";
                return 1;
            }
            if (!w.write(output)) {
                cerr << "Failed to write base64 output\n";
                return 1;
            }
        }
        else {
            auto err = ebm.encode(w);
            if (err) {
                cerr << "Failed to encode EBM: " << err.error<std::string>() << '\n';
                return 1;
            }
        }
        return 0;
    };

    if (flags.output == "-") {
        futils::file::FileStream<std::string> fs{futils::file::File::stdout_file()};
        futils::binary::writer writer{fs.get_write_handler(), &fs};
        if (write_ebm(writer)) {
            return 1;
        }
        if (cout.is_tty()) {  // for web playground
            writer.write("\n");
        }
        TIMING("output");
    }
    else if (!flags.output.empty()) {
        // Serialize and write to output file
        auto file = futils::file::File::create(flags.output);
        if (!file) {
            cerr << "Failed to open output file: " << flags.output << ": " << file.error().error<std::string>() << '\n';
            return 1;
        }

        futils::file::FileStream<std::string> fs{*file};
        futils::binary::writer writer{fs.get_write_handler(), &fs};
        if (write_ebm(writer)) {
            return 1;
        }

        if (flags.verbose) {
            cerr << "ebmgen finished successfully!\n"
                 << "Output written to: " << flags.output << '\n';
        }
        TIMING("output");
    }

    if (flags.print_output_size) {
        size_t size = 0;
        futils::binary::writer w{&futils::binary::discard<>, &size};
        if (auto err = ebm.encode(w); err) {
            cerr << "Failed to encode EBM for size calculation: " << err.error<std::string>() << '\n';
            return 1;
        }
        cerr << "Output size: " << size << " bytes\n";
        TIMING("output size");
    }

    if (flags.query.size() > 0) {
        auto r = ebmgen::run_query(*table, flags.query);
        if (!r) {
            cerr << "Query Error: " << r.error().error<std::string>() << '\n';
            return 1;
        }
        auto& [result, failures] = *r;
        if (flags.query_output_format == QueryOutputFormat::JSON) {
            futils::json::Stringer<> s;
            ebmgen::JSONPrinter printer(*table);
            auto element = s.array();
            for (auto obj : result) {
                auto objv = table->get_object(obj);
                std::visit(
                    [&](auto&& o) {
                        if constexpr (std::is_pointer_v<std::decay_t<decltype(o)>>) {
                            element([&] {
                                printer.print_object(s, *o);
                            });
                        }
                    },
                    objv);
            }
            element.close();
            cout << s.out();
        }
        else if (flags.query_output_format == QueryOutputFormat::Text) {
            std::stringstream ss;
            for (auto obj : result) {
                ebmgen::DebugPrinter printer(*table, ss);
                auto objv = table->get_object(obj);
                std::visit([&](auto&& o) {
                    if constexpr (std::is_pointer_v<std::decay_t<decltype(o)>>) {
                        printer.print_object(*o);
                    }
                },
                           objv);
            }
            cout << ss.str();
            cout << "Total matched objects: " << result.size() << "\n";
        }
        else if (flags.query_output_format == QueryOutputFormat::Hex) {
            for (auto obj : result) {
                auto objv = table->get_object(obj);
                std::visit([&](auto&& o) {
                    if constexpr (std::is_pointer_v<std::decay_t<decltype(o)>>) {
                        std::string buffer;
                        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
                        auto err = o->encode(w);
                        if (err) {
                            cout << "Failed to encode object: " << err.template error<std::string>() << '\n';
                        }
                        else {
                            std::string hex_output;
                            futils::number::hex::to_hex(hex_output, buffer);
                            cout << hex_output << "\n";
                        }
                    }
                },
                           objv);
            }
        }
        else {
            for (auto obj : result) {
                cout << get_id(obj) << '\n';
            }
        }
        if (result.empty() && !failures.empty()) {
            cerr << "Query syntax/semantic error:\n";
            for (auto f : failures) {
                cerr << "  " << to_string(f) << "\n";
            }
        }
        TIMING("query");
    }

    if (flags.interactive) {
        ebmgen::interactive_debugger(*table);
    }

    return 0;
}

int ebmgen_main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags,
        [](auto&& str, bool err) {
            if (err)
                cerr << str;
            else
                cout << str;
        },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <tool/common/em_main.h>
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, ebmgen_main);
}
#else
int main(int argc, char** argv) {
    futils::wrap::U8Arg _(argc, argv);
    futils::wrap::cout_wrap().set_virtual_terminal(true);
    futils::wrap::cerr_wrap().set_virtual_terminal(true);
    return ebmgen_main(argc, argv);
}
#endif