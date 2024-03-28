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
struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool no_color = false;
    bool add_line_map = false;
    bool use_error = false;
    bool use_raw_union = false;
    bool use_overflow_check = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&no_color, "no-color", "disable color output");
        ctx.VarBool(&add_line_map, "add-line-map", "add line map");
        ctx.VarBool(&use_error, "use-error", "use futils::error::Error for error reporting");
        ctx.VarBool(&use_raw_union, "use-raw-union", "use raw union instead of std::variant (maybe unsafe)");
        ctx.VarBool(&use_overflow_check, "use-overflow-check", "add overflow check for integer types");
    }
};

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    prefix_loc() = "json2cpp: ";
    cerr_color_mode = flags.no_color ? ColorMode::no_color : cerr.is_tty() ? ColorMode::force_color
                                                                           : ColorMode::no_color;
    if (flags.spec) {
        cout << R"({
            "input": "file",
            "langs": ["c","json"],
            "suffix": [".h",".c",".json"],
            "separator": "############\n"
        })";
        return 0;
    }
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
    brgen::ast::AstFile f;
    if (!futils::json::convert_from_json(js, f)) {
        print_error("cannot convert json file ", name);
        return 1;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*f.ast);
    if (!res) {
        print_error("cannot decode json file: ", res.error().locations[0].msg);
        return 1;
    }
    if (!*res) {
        print_error("cannot decode json file: ast is null");
        return 1;
    }
    json2c::Generator g;
    auto prog = brgen::ast::cast_to<brgen::ast::Program>(*res);
    namespace fs = std::filesystem;
    auto include_path = fs::path(f.files[0]).filename().replace_extension(".h");
    g.write_program(prog, include_path.generic_string());
    cout << g.h_w.out() << "\n";
    cout << "############\n";
    cout << g.c_w.out() << "\n";
    if (flags.add_line_map) {
        cout << "############\n";
        futils::json::Stringer s;
        {
            auto field = s.object();
            field("structs", g.struct_names);
            field("line_map", g.line_map);
        }
        cout << s.out() << "\n";
    }
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
