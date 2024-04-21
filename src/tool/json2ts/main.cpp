/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <vm/compile.h>
#include "../common/print.h"
#include <file/file_view.h>
#include <core/ast/json.h>
#include <core/ast/file.h>
#include <wrap/argv.h>
#include "generate.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../common/em_main.h"
#endif
#include "../common/load_json.h"
#include "../common/generate.h"

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool javascript = false;
    bool use_bigint = false;
    bool no_resize = false;
    bool no_color = false;
    bool legacy_file_pass = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&javascript, "j,javascript", "javascript mode");
        ctx.VarBool(&use_bigint, "bigint", "use bigint for u64 type (experimental)");
        ctx.VarBool(&no_resize, "no-resize", "not resize output buffer");
        ctx.VarBool(&no_color, "no-color", "no color mode");
        ctx.VarBool(&legacy_file_pass, "f,file", "use legacy file pass mode");
    }
};

int ts_generate(const Flags& flags, brgen::request::GenerateSource& req, std::shared_ptr<brgen::ast::Node> res) {
    auto prog = brgen::ast::cast_to<brgen::ast::Program>(res);
    json2ts::Flags tsflags;
    tsflags.use_bigint = flags.use_bigint;
    tsflags.no_resize = flags.no_resize;
    tsflags.javascript = flags.javascript;
    auto out = json2ts::generate(prog, tsflags);
    send_source(req.id, std::move(out), req.name + (flags.javascript ? ".js" : ".ts"));
    send_end_response(req.id);
    return 0;
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.spec) {
        if (flags.legacy_file_pass) {
            if (flags.javascript) {
                cout << R"({
            "input": "file",
            "langs": ["js"],
            "suffix": [".js"],
            "separator": "############\n"
        })";
            }
            else {
                cout << R"({
            "input": "file",
            "langs": ["ts"],
            "suffix": [".ts"],
            "separator": "############\n"
        })";
            }
        }
        else {
            cout << R"({
                "input": "stdin_stream",
                "langs": ["ts","js"]
            })";
        }
        return 0;
    }
    cerr_color_mode = cerr.is_tty() && !flags.no_color ? ColorMode::force_color : ColorMode::no_color;
    if (flags.legacy_file_pass) {
        return generate_from_file(flags, ts_generate);
    }
    auto handler = [&](brgen::request::GenerateSource& req) {
        do_generate(flags, req, req.json_text, ts_generate);
        return futils::error::Error<>{};
    };
    read_stdin_requests(handler);
}
int json2ts_main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}

#ifdef __EMSCRIPTEN__
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, json2ts_main);
}
#else
int main(int argc, char** argv) {
    futils::wrap::U8Arg _(argc, argv);
    return json2ts_main(argc, argv);
}
#endif
