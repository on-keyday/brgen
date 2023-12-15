/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <core/ast/parse.h>
#include <core/ast/json.h>
#include <wrap/iocommon.h>
#include <console/ansiesc.h>
#include <core/middle/resolve_import.h>
#include <core/middle/resolve_cast.h>
#include <core/middle/resolve_available.h>
#include <core/middle/replace_assert.h>
#include <core/middle/typing.h>
#include <core/middle/type_attribute.h>
#include "../common/print.h"
#include <wrap/argv.h>
#include <core/ast/node_type_list.h>
#include <core/ast/kill_node.h>
#include <wrap/cin.h>
#include <unicode/utf/view.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../common/em_main.h"
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

constexpr auto exit_ok = 0;
constexpr auto exit_err = 1;

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool lexer = false;
    bool not_resolve_import = false;
    bool not_resolve_cast = false;
    bool not_resolve_available = false;
    bool not_resolve_type = false;
    bool not_resolve_assert = false;
    bool not_detect_recursive_type = false;
    bool not_detect_int_set = false;
    bool not_detect_alignment = false;
    bool unresolved_type_as_error = false;

    bool check_ast = false;
    bool dump_types = false;

    bool disable_untyped_warning = false;
    bool disable_unused_warning = false;

    bool print_json = false;
    bool print_on_error = false;
    bool debug_json = false;

    bool no_color = false;

    ColorMode cout_color_mode = ColorMode::auto_color;
    ColorMode cerr_color_mode = ColorMode::auto_color;

    bool stdin_mode = false;
    std::string as_file_name = "<stdin>";

    std::string argv_input;
    bool argv_mode;

    size_t tokenization_limit = 0;

    bool collect_comments = false;

    bool report_error = false;

    bool omit_warning = false;

    brgen::UtfMode input_mode = brgen::UtfMode::utf8;
    brgen::UtfMode interpret_mode = brgen::UtfMode::utf8;

    bool detected_stdio_type = false;

    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&lexer, "l,lexer", "lexer mode");
        ctx.VarBool(&dump_types, "dump-types", "dump types schema mode");
        ctx.VarBool(&check_ast, "c,check-ast", "check ast mode");

        ctx.VarBool(&not_resolve_import, "not-resolve-import", "not resolve import");
        ctx.VarBool(&not_resolve_type, "not-resolve-type", "not resolve type");
        ctx.VarBool(&not_resolve_cast, "not-resolve-cast", "not resolve cast");
        ctx.VarBool(&not_resolve_assert, "not-resolve-assert", "not-resolve assert");
        ctx.VarBool(&not_detect_recursive_type, "not-detect-recursive-type", "not detect recursive type");
        ctx.VarBool(&not_detect_int_set, "not-detect-int-set", "not detect int set");
        ctx.VarBool(&not_detect_alignment, "not-detect-alignment", "not detect alignment");
        ctx.VarBool(&unresolved_type_as_error, "unresolved-type-as-error", "unresolved type as error");

        ctx.VarBool(&disable_untyped_warning, "u,disable-untyped", "disable untyped warning");
        ctx.VarBool(&disable_unused_warning, "disable-unused", "disable unused expression warning");

        ctx.VarBool(&print_json, "p,print-json", "print json of ast/tokens to stdout if succeeded (if stdout is tty. if not tty, usually print json ast)");
        ctx.VarBool(&print_on_error, "print-on-error", "print json of ast/tokens to stdout if failed (if stdout is tty. if not tty, usually print json ast)");
        ctx.VarBool(&omit_warning, "omit-json-warning", "omit warning from json output (if --print-json or --print-on-error)");

        ctx.VarBool(&debug_json, "d,debug-json", "debug mode json output (not parsable ast, only for debug. use with --print-ast)");

        // for compatibility and usability
        ctx.VarBoolFunc(&cerr_color_mode, "no-color", "disable color output (both stdout and stderr)", [&](bool y, auto) {
            if (y) {
                cerr_color_mode = ColorMode::no_color;
                cout_color_mode = ColorMode::no_color;
            }
            else {
                cerr_color_mode = ColorMode::auto_color;
                cout_color_mode = ColorMode::auto_color;
            }
            return true;
        });
        ctx.VarMap<std::string, ColorMode, std::map>(
            &cout_color_mode, "stdout-color", "set color mode for stdout (auto, force, no)", "<mode>",
            std::map<std::string, ColorMode>{
                {"auto", ColorMode::auto_color},
                {"force", ColorMode::force_color},
                {"no", ColorMode::no_color},
            });
        ctx.VarMap<std::string, ColorMode, std::map>(
            &cerr_color_mode, "stderr-color", "set color mode for stderr (auto, force, no)", "<mode>",
            std::map<std::string, ColorMode>{
                {"auto", ColorMode::auto_color},
                {"force", ColorMode::force_color},
                {"no", ColorMode::no_color},
            });

        ctx.VarBool(&stdin_mode, "stdin", "read input from stdin (must not be tty)");
        ctx.VarString(&as_file_name, "stdin-name", "set name of stdin/argv (as a filename)", "<name>");
        ctx.VarString(&argv_input, "argv", "treat cmdline arg as input (this is not designed for human. this is used from other process or emscripten call)", "<source code>");
        ctx.VarInt(&tokenization_limit, "tokenization-limit", "set tokenization limit (use with --lexer) (0=unlimited)", "<size>");

        ctx.VarBool(&collect_comments, "collect-comments", "collect comments");

        ctx.VarMap<std::string, brgen::UtfMode, std::map>(
            &input_mode, "input-mode", "set input mode (default:utf8) (utf8, utf16le, utf16be, utf32le ,utf32be)", "<mode>",
            std::map<std::string, brgen::UtfMode>{
                {"utf8", brgen::UtfMode::utf8},
                {"utf16le", brgen::UtfMode::utf16le},
                {"utf16be", brgen::UtfMode::utf16be},
                {"utf32le", brgen::UtfMode::utf32le},
                {"utf32be", brgen::UtfMode::utf32be},
            });
        ctx.VarMap<std::string, brgen::UtfMode, std::map>(
            &interpret_mode, "interpret-mode", "set interpret mode (default:utf8) (used to decide file position) (utf8, utf16, utf32)", "<mode>",
            std::map<std::string, brgen::UtfMode>{
                {"utf8", brgen::UtfMode::utf8},
                {"utf16", brgen::UtfMode::utf16},
                {"utf32", brgen::UtfMode::utf32},
            });

        ctx.VarBool(&detected_stdio_type, "detected-stdio-type", "detected stdin/stdout/stderr type (for debug)");
    }
};
auto& cout = utils::wrap::cout_wrap();
ColorMode cout_color_mode = ColorMode::auto_color;

auto print_ok() {
    assert(cout_color_mode != ColorMode::auto_color);
    if (!cout.is_tty()) {
        return;
    }
    if (cout_color_mode == ColorMode::force_color) {
        cout << utils::wrap::packln("src2json: ", cse::letter_color<cse::ColorPalette::green>, "ok", cse::color_reset);
    }
    else {
        cout << "src2json: ok\n";
    }
}

auto do_parse(brgen::File* file, bool collect_comments) {
    brgen::ast::Context c;
    c.set_collect_comments(collect_comments);
    return c.enter_stream(file, [&](brgen::ast::Stream& s) {
        return brgen::ast::Parser{s}.parse();
    });
}

auto do_lex(brgen::File* file, size_t limit) {
    brgen::ast::Context c;
    return c.enter_stream(file, [&](brgen::ast::Stream& s) {
        size_t count = 0;
        while (!s.eos()) {
            s.consume();
            ++count;
            if (limit > 0 && count >= limit) {
                print_warning("tokenization limit reached at file ", file->path().generic_u8string());
                break;
            }
        }
        return s.take();
    });
}

int check_ast(std::string_view name, utils::view::rvec view) {
    auto js = utils::json::parse<utils::json::JSON>(view, true);
    if (js.is_undef()) {
        print_error("cannot parse json file ", name);
        return exit_err;
    }
    auto f = js.at("ast");
    if (!f) {
        print_error("cannot find ast field ", name);
        return exit_err;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*f);
    if (!res) {
        print_error("cannot decode json file: ", res.error().locations[0].msg);
        return exit_err;
    }
    print_ok();
    return exit_ok;
}

int dump_types() {
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
            brgen::ast::node_types(list);
        });
        field("struct", [&] {
            auto field = d.object();
            brgen::ast::struct_types(field);
        });
        field("enum", [&] {
            auto field = d.object();
            brgen::ast::enum_types(field);
        });
    }
    cout << d.out();
    if (cout.is_tty()) {
        cout << "\n";
    }
    return exit_ok;
}

int Main(Flags& flags, utils::cmdline::option::Context&) {
    cerr.set_virtual_terminal(true);  // ignore error
    cout.set_virtual_terminal(true);  // ignore error
    if (flags.cout_color_mode == ColorMode::auto_color) {
        if (cout.is_tty()) {
            flags.cout_color_mode = ColorMode::force_color;
        }
        else {
            flags.cout_color_mode = ColorMode::no_color;
        }
    }
    if (flags.cerr_color_mode == ColorMode::auto_color) {
        if (cerr.is_tty()) {
            flags.cerr_color_mode = ColorMode::force_color;
        }
        else {
            flags.cerr_color_mode = ColorMode::no_color;
        }
    }
    cout_color_mode = flags.cout_color_mode;
    cerr_color_mode = flags.cerr_color_mode;
    if (flags.detected_stdio_type) {
        if (auto s = utils::wrap::cin_wrap().get_file().stat()) {
            print_note("detected stdin type: ", to_string(s->mode.type()));
        }
        else {
            print_warning("cannot detect stdin type");
        }
        if (auto s = cout.get_file().stat()) {
            print_note("detected stdout type: ", to_string(s->mode.type()));
        }
        else {
            print_warning("cannot detect stdout type");
        }
        if (auto s = cerr.get_file().stat()) {
            print_note("detected stderr type: ", to_string(s->mode.type()));
        }
        else {
            print_warning("cannot detect stderr type");
        }
    }

    flags.argv_mode = flags.argv_input.size() > 0;

    if (flags.dump_types) {
        return dump_types();
    }

    if (flags.stdin_mode && flags.argv_mode) {
        print_error("cannot use --stdin and --argv at the same time");
        return exit_err;
    }

    if (!flags.stdin_mode && !flags.argv_mode && flags.args.size() == 0) {
        print_error("no input file");
        return exit_err;
    }

    if (flags.args.size() > 1) {
        print_error("only one file is supported now");
        return exit_err;
    }

    std::string name;
    if (flags.stdin_mode || flags.argv_mode) {
        name = flags.as_file_name;
    }
    else {
        name = flags.args[0];
    }

    brgen::FileSet files;
    files.set_utf_mode(flags.input_mode, flags.interpret_mode);
    brgen::File* input = nullptr;
    if (flags.argv_mode) {
        if (flags.input_mode != brgen::UtfMode::utf8) {
            print_error("argv mode only support utf8");
            return exit_err;
        }
        auto ok = files.add_special(name, flags.argv_input);
        if (!ok) {
            print_error("cannot input ", name, " code=", ok.error());
            return exit_err;
        }
        input = files.get_input(*ok);
        if (!input) {
            print_error("cannot input ", name);
            return exit_err;
        }
    }
    else if (flags.stdin_mode) {
        auto& cin = utils::wrap::cin_wrap();
        if (cin.is_tty()) {
            print_error("not support repl mode");
            return exit_err;
        }
        auto& file = cin.get_file();
        std::string file_buf;
        for (;;) {
            utils::byte buffer[1024];
            auto res = file.read_file(buffer);
            if (!res) {
                break;
            }
            file_buf.append(res->as_char(), res->size());
            if (res->size() < 1024) {
                break;
            }
        }
        auto ok = files.add_special(name, std::move(file_buf));
        if (!ok) {
            print_error("cannot input ", name, " code=", ok.error());
            return exit_err;
        }
        input = files.get_input(*ok);
        if (!input) {
            print_error("cannot input ", name);
            return exit_err;
        }
    }
    else {
        auto ok = files.add_file(name);
        if (!ok) {
            print_error("cannot open file ", name, " code=", ok.error());
            return exit_err;
        }
        input = files.get_input(*ok);
        if (!input) {
            print_error("cannot open file ", name);
            return exit_err;
        }
    }

    if (flags.check_ast) {
        if (flags.input_mode != brgen::UtfMode::utf8) {
            print_error("check-ast mode only support utf8");
            return exit_err;
        }
        return check_ast(name, input->source());
    }

    auto dump_json_file = [&](auto&& elem, const char* elem_key, const brgen::SourceError& err) {
        brgen::JSONWriter d;
        {
            auto field = d.object();
            field("files", files.file_list());
            field(elem_key, elem);
            if (err.errs.size() == 0) {
                field("error", nullptr);
            }
            else {
                field("error", err);
            }
        }
        return d;
    };

    auto print_warnings = [&](const brgen::SourceError& err, bool warn_as_error = false) {
        err.for_each_error([&](std::string_view msg, bool w) {
            if (!warn_as_error && w) {
                print_warning(msg);
            }
            else {
                print_error(msg);
            }
        });
    };

    auto report_error = [&](brgen::SourceError&& res, bool warn = false, const char* key = "ast") {
        if (!cout.is_tty() || flags.print_on_error) {
            auto d = dump_json_file(nullptr, key, res);
            cout << utils::wrap::pack(d.out(), cout.is_tty() ? "\n" : "");
        }
        res.for_each_error([&](std::string_view msg, bool w) {
            if (w && !flags.unresolved_type_as_error) {
                print_warning(msg);
            }
            else {
                print_error(msg);
            }
        });
    };

    if (flags.lexer) {
        auto res = do_lex(input, flags.tokenization_limit).transform_error(brgen::to_source_error(files));
        if (!res) {
            report_error(std::move(res.error()), false, "tokens");
            return exit_err;
        }
        if (!cout.is_tty() || flags.print_json) {
            auto d = dump_json_file(*res, "tokens", brgen::SourceError{});
            cout << utils::wrap::pack(d.out(), cout.is_tty() ? "\n" : "");
        }
        else {
            print_ok();
        }
        return exit_ok;
    }

    auto res = do_parse(input, flags.collect_comments).transform_error(brgen::to_source_error(files));

    if (!res) {
        report_error(std::move(res.error()));
        return exit_err;
    }

    const auto kill = [&] {
        brgen::ast::kill_node(*res);
    };

    if (!flags.not_resolve_import) {
        auto res2 = brgen::middle::resolve_import(*res, files).transform_error(brgen::to_source_error(files));
        if (!res2) {
            report_error(std::move(res2.error()));
            return exit_err;
        }
    }
    if (!flags.not_resolve_cast) {
        brgen::middle::resolve_int_cast(*res);
    }
    if (!flags.not_resolve_available) {
        brgen::middle::resolve_available(*res);
    }

    brgen::SourceError err_or_warn;

    if (!flags.not_resolve_type) {
        auto ty = brgen::middle::Typing{};
        auto res3 = ty.typing(*res);
        if (!res3) {
            if (!flags.omit_warning) {
                auto warns = ty.warnings;
                warns.locations.insert(warns.locations.end(), res3.error().locations.begin(), res3.error().locations.end());
                report_error(brgen::to_source_error(files)(std::move(warns)));
            }
            else {
                report_error(brgen::to_source_error(files)(std::move(res3.error())));
            }
            return exit_err;
        }
        if (flags.unresolved_type_as_error && ty.warnings.locations.size() > 0) {
            report_error(brgen::to_source_error(files)(std::move(ty.warnings)));
            return exit_err;
        }
        if (!flags.disable_untyped_warning && ty.warnings.locations.size() > 0) {
            auto warns = brgen::to_source_error(files)(std::move(ty.warnings));
            print_warnings(warns);
            if (!flags.omit_warning) {
                err_or_warn = std::move(warns);
            }
        }
    }

    if (!flags.not_resolve_assert) {
        brgen::LocationError err;
        brgen::middle::replace_assert(err, *res);
        if (!flags.disable_unused_warning && err.locations.size() > 0) {
            print_warnings(brgen::to_source_error(files)(std::move(err)));
            if (!flags.omit_warning) {
                auto tmp = brgen::to_source_error(files)(std::move(err));
                err_or_warn.errs.insert(err_or_warn.errs.end(), tmp.errs.begin(), tmp.errs.end());
            }
        }
    }

    brgen::middle::TypeAttribute attr;

    if (!flags.not_detect_recursive_type) {
        attr.recursive_reference(*res);
    }

    if (!flags.not_detect_int_set) {
        attr.int_type_detection(*res);
    }

    if (!flags.not_detect_alignment) {
        attr.bit_alignment(*res);
    }

    if (cout.is_tty() && !flags.print_json) {
        print_ok();
        return exit_ok;
    }

    brgen::JSONWriter d;
    d.set_no_colon_space(true);
    if (flags.debug_json) {
        d = dump_json_file(*res, "ast", err_or_warn);
    }
    else {
        brgen::ast::JSONConverter c;
        c.obj.set_no_colon_space(true);
        c.encode(*res);
        d = dump_json_file(c.obj, "ast", err_or_warn);
    }
    cout << utils::wrap::pack(d.out(), cout.is_tty() ? "\n" : "");

    return exit_ok;
}

int src2json_main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags,
        [&](auto&& str, bool err) {
            if (err) {
                if (flags.cerr_color_mode == ColorMode::auto_color) {
                    if (cerr.is_tty()) {
                        flags.cerr_color_mode = ColorMode::force_color;
                    }
                    else {
                        flags.cerr_color_mode = ColorMode::no_color;
                    }
                }
                cerr_color_mode = flags.cerr_color_mode;
                print_error(str);
            }
            else {
                cout << str;
            }
        },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        },
        true);
}

#ifdef __EMSCRIPTEN__
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    return em_main(cmdline, src2json_main);
}
#else

int main(int argc, char** argv) {
    utils::wrap::U8Arg _(argc, argv);
    return src2json_main(argc, argv);
}
#endif
