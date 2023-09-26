/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include "../common/print.h"
#include <core/ast/json.h>
#include <file/file_view.h>
#include "gen.h"

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
    }
};
auto& cout = utils::wrap::cout_wrap();

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    if (flags.spec) {
        cout << R"({
            "pass_by": "file",
            "langs": ["cpp"] 
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
    json2cpp::Generator g(brgen::ast::cast_to<brgen::ast::Program>(*res));
    auto res2 = g.generate();
    if (!res2) {
        print_error("cannot generate code: ", res.error().locations[0].msg);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
