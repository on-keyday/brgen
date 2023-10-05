/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <core/ast/parse.h>
#include <core/ast/json.h>
#include <wrap/iocommon.h>
#include <console/ansiesc.h>
#include <core/middle/resolve_import.h>
#include <core/middle/typing.h>
#include "common/print.h"
#include <wrap/argv.h>
#include <core/ast/node_type_list.h>
#include <core/ast/kill_node.h>

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool not_resolve_import = false;
    bool check_ast = false;
    bool dump_types = false;
    bool not_resolve_type = false;
    bool disable_untyped_warning = false;
    bool print_ast = false;
    bool debug_json = false;
    bool dump_ptr_as_uintptr = false;
    bool flat = false;
    bool not_dump_base = false;

    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&not_resolve_import, "not-resolve-import", "not resolve import");
        ctx.VarBool(&check_ast, "c,check-ast", "check ast mode");
        ctx.VarBool(&not_resolve_type, "not-resolve-type", "not resolve type");
        ctx.VarBool(&disable_untyped_warning, "u,disable-untyped", "disable untyped warning");
        ctx.VarBool(&print_ast, "p,print-ast", "print ast to stdout if succeeded (if stdout is tty. if not tty, usually print json ast)");
        ctx.VarBool(&debug_json, "d,debug-json", "debug mode json output (not parsable ast, only for debug. use with --print-ast)");
        ctx.VarBool(&dump_types, "dump-ast", "dump ast types schema mode");
        ctx.VarBool(&dump_ptr_as_uintptr, "dump-uintptr", "make pointer type of ast field uintptr (use with --dump-ast)");
        ctx.VarBool(&flat, "dump-flat", "dump ast schema with flat body (use with --dump-ast)");
        ctx.VarBool(&not_dump_base, "not-dump-base", "not dump ast schema with base type (use with --dump-ast)");
    }
};
auto& cout = utils::wrap::cout_wrap();

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
    auto js = utils::json::parse<utils::json::JSON>(view, true);
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

int node_list(bool dump_uintptr, bool flat, bool not_dump_base) {
    brgen::JSONWriter d;
    {
        auto field = d.object();
        field("node", [&] {
            auto field = d.array();
            auto list = [&](auto&& objdump) {
                field([&] {
                    auto field = d.object();
                    objdump([&](auto key, auto value) {
                        field(key, value);
                    });
                });
            };
            if (dump_uintptr) {
                brgen::ast::node_type_list<true>(list, flat, !not_dump_base);
            }
            else {
                brgen::ast::node_type_list(list, flat, !not_dump_base);
            }
        });
        brgen::ast::custom_type_mapping(field, dump_uintptr);
    }
    cout << d.out();
    if (cout.is_tty()) {
        cout << "\n";
    }
    return 0;
}

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    utils::wrap::out_virtual_terminal = true;

    if (flags.dump_types) {
        return node_list(flags.dump_types, flags.flat, flags.not_dump_base);
    }

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
        print_error("cannot open file ", name, " code=", ok.error());
        return -1;
    }
    auto input = files.get_input(*ok);
    if (!input) {
        print_error("cannot open file ", name);
        return -1;
    }

    auto report_error = [&](auto&& res, bool warn = false) {
        if (!warn && !cout.is_tty()) {
            brgen::JSONWriter d;
            {
                auto field = d.object();
                field("file", files.file_list());
                field("ast", nullptr);
                field("error", res.to_string());
            }
            cout << d.out();
        }
        res.for_each_error([&](std::string_view msg, bool warn) {
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
        report_error(res.error());
        return -1;
    }

    const auto kill = [&] {
        brgen::ast::kill_node(*res);
    };

    if (!flags.not_resolve_import) {
        auto res2 = brgen::middle::resolve_import(*res, files).transform_error(brgen::to_source_error(files));
        if (!res2) {
            report_error(res2.error());
            return -1;
        }
    }
    if (!flags.not_resolve_type) {
        auto ty = brgen::middle::Typing{};
        auto res3 = ty.typing(*res).transform_error(brgen::to_source_error(files));
        if (!res3) {
            report_error(res3.error());
            return -1;
        }
        if (!flags.disable_untyped_warning && ty.warnings.locations.size() > 0) {
            report_error(brgen::to_source_error(files)(std::move(ty.warnings)), true);
        }
    }

    if (cout.is_tty() && !flags.print_ast) {
        cout << cse::letter_color<cse::ColorPalette::green> << "ok" << cse::color_reset << "\n";
        return 0;
    }

    brgen::JSONWriter d;
    d.set_no_colon_space(true);
    if (flags.debug_json) {
        auto field = d.object();
        field("file", files.file_list());
        field("ast", res.value());
        field("error", nullptr);
    }
    else {
        auto field = d.object();
        brgen::ast::JSONConverter c;
        c.obj.set_no_colon_space(true);
        field("file", files.file_list());
        c.encode(*res);
        field("ast", c.obj);
        field("error", nullptr);
    }
    cout << d.out();
    if (cout.is_tty()) {
        cout << "\n";
    }

    return 0;
}

int main(int argc, char** argv) {
    utils::wrap::U8Arg _(argc, argv);
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cerr << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
