/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include "../common/print.h"
#include <core/ast/json.h>
#include <core/ast/file.h>
#include <file/file_view.h>
#include <filesystem>
#include "generate.h"
#include <wrap/argv.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../common/em_main.h"
#endif
#include "../common/generate.h"

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool no_color = false;
    bool add_line_map = false;
    bool use_overflow_check = false;
    bool single = false;
    bool omit_error_callback = false;
    bool use_memcpy = false;
    bool zero_copy = false;
    bool legacy_file_pass = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&no_color, "no-color", "disable color output");
        ctx.VarBool(&add_line_map, "add-line-map", "add line map");
        ctx.VarBool(&use_overflow_check, "use-overflow-check", "add overflow check for integer types");
        ctx.VarBool(&single, "single", "output as single file");
        ctx.VarBool(&omit_error_callback, "omit-error-callback", "omit error callback");
        ctx.VarBool(&zero_copy, "zero-copy", "use zero copy for decoding byte array");
        ctx.VarBool(&use_memcpy, "use-memcpy", "use memcpy instead of `for` loop for byte array copy");
        ctx.VarBool(&legacy_file_pass, "f,file", "use legacy file pass mode");
    }
};

int generate_c(const Flags& flags, brgen::request::GenerateSource& req, std::shared_ptr<brgen::ast::Node> res) {
    json2c::Generator g;
    g.omit_error_callback = flags.omit_error_callback;
    g.zero_copy_optimization = flags.zero_copy;
    g.use_memcpy = flags.use_memcpy;
    auto prog = brgen::ast::cast_to<brgen::ast::Program>(res);
    namespace fs = std::filesystem;
    g.write_program(prog, flags.single ? "" : req.name + ".h");
    if (flags.single) {
        send_source(req.id, std::move(g.h_w.out()) + std::move(g.c_w.out()), req.name + ".c");
    }
    else {
        send_source(req.id, std::move(g.h_w.out()), req.name + ".h");
        if (send_as_text) {
            cout << "############\n";
        }
        send_source(req.id, std::move(g.c_w.out()), req.name + ".c");
    }
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
        send_source(req.id, std::move(s.out()), req.name + ".c.map.json");
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
            if (flags.single) {
                cout << R"({
            "input": "file",
            "langs": ["c","json"],
            "suffix": [".c",".json"],
            "separator": "############\n"
        })";
            }
            else {
                cout << R"({
            "input": "file",
            "langs": ["c","json"],
            "suffix": [".h",".c",".json"],
            "separator": "############\n"
        })";
            }
        }
        else {
            cout << R"({
            "input": "stdin_stream",
            "langs": ["c","json"]
            })";
        }
        return 0;
    }
    if (flags.legacy_file_pass) {
        return generate_from_file(flags, generate_c);
    }
    read_stdin_requests([&](brgen::request::GenerateSource& req) {
        do_generate(flags, req, req.json_text, generate_c);
        return futils::error::Error<>{};
    });
    return 0;
}

int json2c_main(int argc, char** argv) {
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
    return em_main(cmdline, json2c_main);
}
#else
int main(int argc, char** argv) {
    return json2c_main(argc, argv);
}
#endif
