/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <core/ast/parse.h>
#include <core/ast/json.h>
#include <wrap/iocommon.h>
#include <console/ansiesc.h>
#include <future>
#include <wrap/cin.h>
#include <core/middle/resolve_import.h>

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool not_resolve_import = false;
    bool check_ast = false;

    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&not_resolve_import, "not-resolve-import", "not resolve import");
        ctx.VarBool(&check_ast, "check-ast", "check ast mode");
    }
};
auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();
namespace cse = utils::console::escape;
void print_error(auto&&... msg) {
    auto p = utils::wrap::pack();
    p << "src2json: ";
    if (cerr.is_tty()) {
        p << cse::letter_color_code<9>;
    }
    p << "error: ";
    if (cerr.is_tty()) {
        p << cse::letter_color<cse::ColorPalette::white>;
    }
    (p << ... << msg);
    if (cerr.is_tty()) {
        p << "\n"
          << cse::color_reset;
    }
    cerr << p.pack();
}

void print_warning(auto&&... msg) {
    auto p = utils::wrap::pack();
    p << "src2json: ";
    if (cerr.is_tty()) {
        p << cse::letter_color<cse::ColorPalette::yellow>;
    }
    p << "warning: ";
    if (cerr.is_tty()) {
        p << cse::letter_color<cse::ColorPalette::white>;
    }
    (p << ... << msg);
    if (cerr.is_tty()) {
        p << "\n"
          << cse::color_reset;
    }
    cerr << p.pack();
}

auto do_parse(brgen::File* file) {
    brgen::ast::Context c;
    return c.enter_stream(file, [&](brgen::ast::Stream& s) {
        return brgen::ast::Parser{s}.parse();
    });
}

int check_ast(std::string_view name) {
    utils::file::View view;
    if (!view.open(name)) {
        print_error("cannot open file ", name);
        return -1;
    }
    auto js = utils::json::parse<utils::json::JSON>(view);
    if (js.is_undef()) {
        print_error("cannot parse json file ", name);
        return -1;
    }
    auto f = js.at("ast");
    if (!f) {
        print_error("cannot find ast field ", name);
        return -1;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*f);
    if (!res) {
        print_error("cannot decode json file: ", res.error().locations[0].msg);
        return -1;
    }
    cout << "ok\n";
    return 0;
}

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    utils::wrap::out_virtual_terminal = true;

    if (flags.args.size() == 0) {
        print_error("no input file");
        return -1;
    }

    if (flags.args.size() > 1) {
        print_error("only one file is supported now");
        return -1;
    }
    auto name = flags.args[0];

    if (flags.check_ast) {
        return check_ast(name);
    }

    brgen::FileSet files;

    auto ok = files.add(name);
    if (!ok) {
        print_error("cannot open file  ", name, " code=", ok.error());
        return -1;
    }
    auto input = files.get_input(*ok);
    if (!input) {
        print_error("cannot open file  ", name);
        return -1;
    }

    auto report_error = [&](auto& res) {
        brgen::Debug d;
        {
            auto field = d.object();
            field("file", files.file_list());
            field("ast", nullptr);
            field("error", res.error().to_string());
        }
        res.error().for_each_error([&](std::string_view msg, bool warn) {
            if (warn) {
                print_warning(msg);
            }
            else {
                print_error(msg);
            }
        });
    };
    auto res = do_parse(input).transform_error(brgen::to_source_error(files));
    if (!res) {
        report_error(res);
        return -1;
    }
    if (!flags.not_resolve_import) {
        auto res2 = brgen::middle::resolve_import(*res, files).transform_error(brgen::to_source_error(files));
        if (!res2) {
            report_error(res2);
            return -1;
        }
    }
    brgen::Debug d;
    bool has_error = false;
    {
        auto field = d.object();
        brgen::ast::JSONConverter c;
        field("file", files.file_list());
        c.encode(*res);
        field("ast", c.obj);
        field("error", nullptr);
    }
    if (!cout.is_tty() || !has_error) {
        cout << d.out();
        if (cout.is_tty()) {
            cout << "\n";
        }
    }
    return has_error ? -1 : 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
    utils::wrap::cin_wrap().has_input();
}
