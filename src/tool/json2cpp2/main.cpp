/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include "../common/print.h"
#include "../common/send.h"
#include "../common/load_json.h"
#include <core/ast/json.h>
#include <json/convert_json.h>
#include <core/ast/file.h>
#include <file/file_view.h>
#include "generate.h"
#include <wrap/argv.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../common/em_main.h"
#endif
#include <request/stream.hpp>
#include <filesystem>
#include "../common/generate.h"

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool no_color = false;
    bool add_line_map = false;
    bool use_error = false;
    bool use_raw_union = false;
    bool use_overflow_check = false;
    bool legacy_file_pass = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&no_color, "no-color", "disable color output");
        ctx.VarBool(&add_line_map, "add-line-map", "add line map");
        ctx.VarBool(&use_error, "use-error", "use futils::error::Error for error reporting");
        ctx.VarBool(&use_raw_union, "use-raw-union", "use raw union instead of std::variant (maybe unsafe)");
        ctx.VarBool(&use_overflow_check, "use-overflow-check", "add overflow check for integer types");
        ctx.VarBool(&legacy_file_pass, "f,file", "use legacy file pass mode");
    }
};

int cpp_generate(const Flags& flags, brgen::request::GenerateSource& req, std::shared_ptr<brgen::ast::Node> res) {
    j2cp2::Generator g;
    g.enable_line_map = flags.add_line_map;
    g.use_error = flags.use_error;
    g.use_variant = !flags.use_raw_union;
    g.use_overflow_check = flags.use_overflow_check;
    auto prog = brgen::ast::cast_to<brgen::ast::Program>(res);
    g.write_program(prog);
    send_source(req.id, std::move(g.w.out()), req.name + ".hpp");
    if (flags.add_line_map) {
        if (send_as_text) {
            cout << "############\n";
        }
        futils::json::Stringer s;
        {
            auto field = s.object();
            field("structs", g.struct_names);
            field("line_map", g.line_map);
        }
        send_source(req.id, std::move(s.out()), req.name + ".hpp.map.json");
    }
    send_end_response(req.id);
    return 0;
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    prefix_loc() = "json2cpp: ";
    cerr_color_mode = flags.no_color ? ColorMode::no_color : cerr.is_tty() ? ColorMode::force_color
                                                                           : ColorMode::no_color;
    if (flags.spec) {
        if (flags.legacy_file_pass) {
            cout << R"({
            "input": "file",
            "langs": ["cpp","json"],
            "suffix": [".hpp",".json"],
            "separator": "############\n"
        })";
        }
        else {
            cout << R"({
            "input": "stdin_stream",
            "langs": ["cpp","json"]
            })";
        }
        return 0;
    }
    if (flags.legacy_file_pass) {
        return generate_from_file(flags, cpp_generate);
    }
    read_stdin_requests([&](brgen::request::GenerateSource& req) {
        do_generate(flags, req, req.json_text, cpp_generate);
        return futils::error::Error<>{};
    });
    return 0;
}

int json2cpp_main(int argc, char** argv) {
    futils::wrap::U8Arg _(argc, argv);
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cerr << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}

#ifdef __EMSCRIPTEN__
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, json2cpp_main);
}
#else
int main(int argc, char** argv) {
    return json2cpp_main(argc, argv);
}
#endif
