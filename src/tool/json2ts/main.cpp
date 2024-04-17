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

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool javascript = false;
    bool use_bigint = false;
    bool no_resize = false;
    bool no_color = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&javascript, "j,javascript", "javascript mode");
        ctx.VarBool(&use_bigint, "bigint", "use bigint for u64 type (experimental)");
        ctx.VarBool(&no_resize, "no-resize", "not resize output buffer");
        ctx.VarBool(&no_color, "no-color", "no color mode");
    }
};

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.spec) {
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
        return 0;
    }
    cerr_color_mode = cerr.is_tty() && !flags.no_color ? ColorMode::force_color : ColorMode::no_color;
    if (flags.args.empty()) {
        print_error("no input file");
        return 1;
    }
    if (flags.args.size() > 1) {
        print_error("too many input files");
        return 1;
    }
    auto name = flags.args[0];
    futils::file::View view;
    if (!view.open(name)) {
        print_error("cannot open file ", name);
        return 1;
    }
    auto js = futils::json::parse<futils::json::JSON>(view);
    if (js.is_undef()) {
        print_error("cannot parse json file ", name);
        return 1;
    }
    brgen::ast::AstFile file;
    if (!futils::json::convert_from_json(js, file)) {
        print_error("cannot convert json file to ast: ", name);
        return 1;
    }
    if (!file.ast) {
        print_error("cannot convert json file to ast: ast is null");
        return 1;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*file.ast);
    if (!res) {
        print_error("cannot decode json file: ", res.error().locations[0].msg);
        return 1;
    }
    if (!*res) {
        print_error("cannot decode json file: ast is null");
        return 1;
    }
    json2ts::Flags tsflags;
    tsflags.use_bigint = flags.use_bigint;
    tsflags.no_resize = flags.no_resize;
    tsflags.javascript = flags.javascript;
    auto out = json2ts::generate(brgen::ast::cast_to<brgen::ast::Program>(*res), tsflags);
    cout << out << "\n";
    return 0;
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
