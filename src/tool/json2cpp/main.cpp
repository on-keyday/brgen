/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include "../common/print.h"
#include <core/ast/json.h>
#include <file/file_view.h>
#include "gen.h"
#include <wrap/argv.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../common/em_main.h"
#endif
struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool no_color = false;
    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&no_color, "no-color", "disable color output");
    }
};
auto& cout = utils::wrap::cout_wrap();

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    prefix_loc() = "json2cpp: ";
    no_color = flags.no_color;
    if (flags.spec) {
        cout << R"({
            "pass_by": "file",
            "langs": ["cpp"],
            "suffix": ".hpp"
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
    utils::file::View view;
    if (!view.open(name)) {
        print_error("cannot open file ", name);
        return 1;
    }
    auto js = utils::json::parse<utils::json::JSON>(view);
    if (js.is_undef()) {
        print_error("cannot parse json file ", name);
        return 1;
    }
    auto files = js.at("files");
    if (!files) {
        print_error("cannot find files field ", name);
        return 1;
    }
    if (!files->is_array()) {
        print_error("files field is not array ", name);
        return 1;
    }
    auto f = js.at("ast");
    if (!f) {
        print_error("cannot find ast field ", name);
        return 1;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*f);
    if (!res) {
        print_error("cannot decode json file: ", res.error().locations[0].msg);
        return 1;
    }
    if (!*res) {
        print_error("cannot decode json file: ast is null");
        return 1;
    }
    json2cpp::Generator g(brgen::ast::cast_to<brgen::ast::Program>(*res));
    auto res2 = g.generate();
    if (!res2) {
        auto& err = res2.error();
        auto loc = err.loc;
        std::string name;
        if (loc.file == 0) {
            name = "<unknown file>";
        }
        else {
            auto l = files->at(loc.file - 1);
            if (!l || !l->is_string()) {
                name = "<unknown file>";
            }
            else {
                l->as_string(name);
            }
        }
        print_error("cannot generate code: ", err.msg, " at ", name, ":", loc.line, ":", loc.col);
        return 1;
    }
    cout << g.code.out();
    return 0;
}

int json2cpp_main(int argc, char** argv) {
    utils::wrap::U8Arg _(argc, argv);
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cerr << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
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
