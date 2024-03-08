/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <vm/compile.h>
#include "../common/print.h"
#include <file/file_view.h>
#include <core/ast/json.h>
#include <core/ast/file.h>
#include "generate.h"

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool javascript = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&javascript, "j,javascript", "javascript mode");
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
    cerr_color_mode = cerr.is_tty() ? ColorMode::force_color : ColorMode::no_color;
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
        print_error("cannot convert json file to ast: ");
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

    auto out = json2ts::generate(brgen::ast::cast_to<brgen::ast::Program>(*res), flags.javascript);
    cout << out << "\n";
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
