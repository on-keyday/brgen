/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <core/ast/parse.h>
#include <core/ast/json.h>
#include <wrap/iocommon.h>
#include <console/ansiesc.h>
#include <core/middle/resolve_import.h>
#include <core/middle/resolve_available.h>
#include <core/middle/replace_assert.h>
#include <core/middle/replace_order_spec.h>
#include <core/middle/replace_error.h>
#include <core/middle/resolve_io_operation.h>
#include <core/middle/replace_metadata.h>
#include <core/middle/resolve_state_dependency.h>
#include <core/middle/typing.h>
#include <core/middle/type_attribute.h>
#include "../common/print.h"
#include <core/ast/node_type_list.h>
#include <core/ast/kill_node.h>
#include <wrap/cin.h>
#include <unicode/utf/view.h>
#include <json/convert_json.h>
#include <core/ast/file.h>
#ifdef SRC2JSON_DLL
#include "hook.h"
#include "capi_export.h"
#endif

#include "version.h"

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string_view> args;
    bool version = false;
    bool lexer = false;

    bool not_resolve_import = false;
    bool not_resolve_available = false;
    bool not_resolve_type = false;
    bool not_resolve_assert = false;
    bool not_resolve_endian_spec = false;
    bool not_resolve_explicit_error = false;
    bool not_resolve_io_operation = false;
    bool not_detect_recursive_type = false;
    bool not_detect_non_dynamic = false;
    bool not_detect_alignment = false;
    bool not_resolve_state_dependency = false;
    bool not_resolve_metadata = false;

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
    std::string_view as_file_name = "<stdin>";

    std::string_view argv_input;
    bool argv_mode;

    size_t tokenization_limit = 0;

    bool collect_comments = false;

    bool report_error = false;

    bool omit_warning = false;

    brgen::UtfMode input_mode = brgen::UtfMode::utf8;
    brgen::UtfMode interpret_mode = brgen::UtfMode::utf8;

    bool detected_stdio_type = false;

    bool spec = false;
    bool force_ok = false;

    bool via_http = false;
    std::string port = "8080";
    bool check_http = false;
    bool use_unsafe_escape = false;

    // std::string_view error_diagnostic;

    bool error_tolerant = false;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&version, "version", "print version");
        ctx.VarBool(&spec, "s,spec", "print spec of src2json (for generator mode)");
        ctx.VarBool(&force_ok, "force-ok", "force print ok when succeeded (for generator mode) (for debug)");
        ctx.VarBool(&lexer, "l,lexer", "lexer mode");
        ctx.VarBool(&dump_types, "dump-types", "dump types schema mode");
        ctx.VarBool(&check_ast, "c,check-ast", "check ast mode");

        ctx.VarBool(&not_resolve_import, "not-resolve-import", "not resolve import");
        ctx.VarBool(&not_resolve_type, "not-resolve-type", "not resolve type");
        ctx.VarBool(&not_resolve_assert, "not-resolve-assert", "not resolve assert");
        ctx.VarBool(&not_resolve_available, "not-resolve-available", "not resolve available");
        ctx.VarBool(&not_resolve_endian_spec, "not-resolve-endian-spec", "not resolve endian-spec");
        ctx.VarBool(&not_detect_recursive_type, "not-detect-recursive-type", "not detect recursive type");
        ctx.VarBool(&not_detect_non_dynamic, "not-detect-non-dynamic", "not detect non-dynamic type");
        ctx.VarBool(&not_detect_alignment, "not-detect-alignment", "not detect alignment");
        ctx.VarBool(&not_resolve_explicit_error, "not-resolve-explicit-error", "not resolve explicit error");
        ctx.VarBool(&not_resolve_io_operation, "not-resolve-io-operation", "not resolve io operation");
        ctx.VarBool(&not_resolve_state_dependency, "not-resolve-state-dependency", "not resolve state dependency");

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
        ctx.VarString<true>(&as_file_name, "stdin-name", "set name of stdin/argv (as a filename)", "<name>");
        ctx.VarString<true>(&argv_input, "argv", "treat cmdline arg as input (this is not designed for human. this is used from other process or emscripten call)", "<source code>");
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

        ctx.VarBool(&via_http, "via-http", "run as http server (POST /parse endpoint for src2json ({\"args\": []} for argument), GET /stop to stop server)");
        ctx.VarString(&port, "port", "set port of http server", "<port>");
        ctx.VarBool(&check_http, "check-http", "check http mode is enabled (for debug)");
        ctx.VarBool(&use_unsafe_escape, "unsafe-escape", "use unsafe escape (this flag make json escape via http unsafe; ansi color escape sequence is not escaped)");

        ctx.VarBool(&error_tolerant, "error-tolerant", "error tolerant mode (for lsp) (experimental)");
    }
};

thread_local ColorMode cout_color_mode = ColorMode::auto_color;
thread_local bool force_print_ok = false;

auto print_ok() {
    assert(cout_color_mode != ColorMode::auto_color);
    if (!cout.is_tty() && !force_print_ok) {
        return;
    }
    if (cout_color_mode == ColorMode::force_color) {
        cout << futils::wrap::packln("src2json: ", cse::letter_color<cse::ColorPalette::green>, "ok", cse::color_reset);
    }
    else {
        cout << "src2json: ok\n";
    }
}

auto do_parse(brgen::File* file, Flags& flags) {
    brgen::ast::Context c;
    c.set_collect_comments(flags.collect_comments);

    return c.enter_stream(file, [&](brgen::ast::Stream& s) {
        auto p = brgen::ast::Parser{s};
        p.state.error_tolerant = flags.error_tolerant;
        auto res = p.parse();
        return res;
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

int check_ast(std::string_view name, futils::view::rvec view) {
    auto js = futils::json::parse<futils::json::JSON>(view, true);
    if (js.is_undef()) {
        print_error("cannot parse json file ", name);
        return exit_err;
    }
    brgen::ast::AstFile file;
    if (!futils::json::convert_from_json(js, file)) {
        print_error("cannot convert json file ", name);
        return exit_err;
    }
    if (!file.ast) {
        print_warning("ast is null");
        return exit_err;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*file.ast);
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

struct DiagnosticInfo {
    std::string message;
    size_t begin;
    size_t end;
    size_t line;
    size_t col;

    bool from_json(auto&& js) {
        JSON_PARAM_BEGIN(*this, js)
        FROM_JSON_PARAM(message, "msg")
        FROM_JSON_PARAM(begin, "begin")
        FROM_JSON_PARAM(end, "end")
        FROM_JSON_PARAM(line, "line")
        FROM_JSON_PARAM(col, "col")
        JSON_PARAM_END()
    }
};

auto dump_json_file(brgen::FileSet& files, bool ok, auto&& elem, const char* elem_key, brgen::LocationError&& err) {
    auto src_err = brgen::to_source_error(files)(std::move(err));
    brgen::JSONWriter d;
    {
        auto field = d.object();
        // when error tolerant, ast may not be null even if error exists
        // so, we add success field
        field("success", ok);
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

int print_spec(Flags& flags) {
    if (flags.check_ast && flags.stdin_mode && !flags.via_http) {
        cout << R"({
    "langs": ["log"],
    "input": "stdin",
    "suffix": [".log"]
})";
    }
    else {
        print_error("spec mode only support check-ast with stdin mode");
        return exit_err;
    }
    return exit_ok;
}

int parse_and_analyze(std::shared_ptr<brgen::ast::Program>* p, brgen::FileSet& files, brgen::File* input, Flags& flags, const Capability& cap, brgen::LocationError& err_or_warn) {
    assert(p);
    auto to_src_error = brgen::to_source_error(files);
    auto res = do_parse(input, flags);

    if (!res) {
        report_error(to_src_error(std::move(res.error())));
        return exit_err;
    }

    const auto kill = futils::helper::defer([&] {
        brgen::ast::kill_node(*res);
    });

    if (!flags.not_resolve_import) {
        if (!cap.importer) {
            print_error("import is disabled");
            return exit_err;
        }
        auto res2 = brgen::middle::resolve_import(*res, files).transform_error(brgen::to_source_error(files));
        if (!res2) {
            report_error(std::move(res2.error()));
            return exit_err;
        }
    }

    if (!flags.not_resolve_available) {
        brgen::middle::resolve_available(*res);
    }

    brgen::LocationError err_or_warn;

    if (!flags.not_resolve_endian_spec) {
        brgen::middle::replace_specify_order(*res);
    }

    if (!flags.not_resolve_explicit_error) {
        auto res2 = brgen::middle::replace_explicit_error(*res);
        if (!res2) {
            report_error(brgen::to_source_error(files)(std::move(res2.error())));
            return exit_err;
        }
    }

    if (!flags.not_resolve_io_operation) {
        auto res2 = brgen::middle::resolve_io_operation(*res);
        if (!res2) {
            report_error(brgen::to_source_error(files)(std::move(res2.error())));
            return exit_err;
        }
    }

    if (!flags.not_resolve_metadata) {
        brgen::middle::replace_metadata(*res);
    }

    if (!flags.not_resolve_type) {
        brgen::LocationError warns;
        auto res3 = brgen::middle::analyze_type(*res, warns);
        if (!res3) {
            if (!flags.omit_warning) {
                warns.locations.insert(warns.locations.end(), res3.error().locations.begin(), res3.error().locations.end());
                report_error(brgen::to_source_error(files)(std::move(warns)));
            }
            else {
                report_error(brgen::to_source_error(files)(std::move(res3.error())));
            }
            return exit_err;
        }
        if (flags.unresolved_type_as_error && err_or_warn.locations.size() > 0) {
            report_error(brgen::to_source_error(files)(std::move(warns)));
            return exit_err;
        }
        if (!flags.disable_untyped_warning && err_or_warn.locations.size() > 0) {
            auto warns = brgen::to_source_error(files)(std::move(warns));
            print_warnings(warns);
        }
    }

    if (!flags.not_resolve_assert) {
        brgen::LocationError err;
        brgen::middle::replace_assert(*res, err);
        if (!flags.disable_unused_warning && err.locations.size() > 0) {
            auto tmp = brgen::to_source_error(files)(std::move(err));
            print_warnings(tmp);
            if (!flags.omit_warning) {
                err_or_warn.errs.insert(err_or_warn.errs.end(), tmp.errs.begin(), tmp.errs.end());
            }
        }
    }

    brgen::middle::TypeAttribute attr;

    if (!flags.not_detect_recursive_type) {
        attr.mark_recursive_reference(*res);
    }

    if (!flags.not_detect_non_dynamic) {
        attr.detect_non_dynamic_type(*res);
    }

    if (!flags.not_detect_alignment) {
        attr.analyze_bit_size_and_alignment(*res);
    }

    if (!flags.not_resolve_state_dependency) {
        brgen::middle::resolve_state_dependency(*res);
    }
}

int Main(Flags& flags, futils::cmdline::option::Context&, const Capability& cap) {
    if (flags.version) {
        cout << futils::wrap::pack("src2json version ", src2json_version, " (lang version ", lang_version, ")\n");
        return exit_ok;
    }
    if (flags.spec) {
        return print_spec(flags);
    }

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
    force_print_ok = flags.force_ok;
    if (flags.via_http) {
        if (!cap.network) {
            print_error("network mode is disabled");
            return exit_err;
        }
#ifdef S2J_USE_NETWORK
        return network_main(flags.port.c_str(), flags.use_unsafe_escape, cap);
#else
        print_error("network mode is not supported");
        return exit_err;
#endif
    }
    if (flags.check_http) {
#ifdef S2J_USE_NETWORK
        print_ok();
        return exit_ok;
#else
        print_error("network mode is not supported");
        return exit_err;
#endif
    }
    if (flags.detected_stdio_type) {
        if (auto s = futils::wrap::cin_wrap().get_file().stat()) {
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

    std::string_view name;
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
        if (!cap.argv) {
            print_error("argv mode is disabled");
            return exit_err;
        }
        if (flags.input_mode != brgen::UtfMode::utf8) {
            print_error("argv mode only support utf8 for --input-mode");
            return exit_err;
        }
        auto ok = files.add_special(name, flags.argv_input);
        if (!ok) {
            print_error("cannot input ", name, " ", brgen::to_error_message(ok.error()));
            return exit_err;
        }
        input = files.get_input(*ok);
        if (!input) {
            print_error("cannot input ", name);
            return exit_err;
        }
    }
    else if (flags.stdin_mode) {
        if (!cap.std_input) {
            print_error("stdin mode is disabled");
            return exit_err;
        }
        auto& cin = futils::wrap::cin_wrap();
        if (cin.is_tty()) {
            print_error("not support repl mode");
            return exit_err;
        }
        auto& file = cin.get_file();
        std::string file_buf;
        for (;;) {
            futils::byte buffer[1024];
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
            print_error("cannot input ", name, " ", brgen::to_error_message(ok.error()));
            return exit_err;
        }
        input = files.get_input(*ok);
        if (!input) {
            print_error("cannot input ", name);
            return exit_err;
        }
    }
    else {
        if (!cap.file) {
            print_error("file mode is disabled");
            return exit_err;
        }
        auto ok = files.add_file(name);
        if (!ok) {
            print_error("cannot open file ", name, " ", brgen::to_error_message(ok.error()));
            return exit_err;
        }
        input = files.get_input(*ok);
        if (!input) {
            print_error("cannot open file ", name);
            return exit_err;
        }
    }

    if (flags.check_ast) {
        if (!cap.check_ast) {
            print_error("--check-ast mode is disabled");
            return exit_err;
        }
        if (flags.input_mode != brgen::UtfMode::utf8) {
            print_error("--check-ast mode only support utf8 for --input-mode");
            return exit_err;
        }
        return check_ast(name, input->source());
    }

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
            auto d = dump_json_file(files, nullptr, key, res);
            cout << futils::wrap::pack(d.out(), cout.is_tty() ? "\n" : "");
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
        if (!cap.lexer) {
            print_error("lexer mode is disabled");
            return exit_err;
        }
        auto res = do_lex(input, flags.tokenization_limit).transform_error(brgen::to_source_error(files));
        if (!res) {
            report_error(std::move(res.error()), false, "tokens");
            return exit_err;
        }
        if (!cout.is_tty() || flags.print_json) {
            auto d = dump_json_file(files, true, *res, "tokens", brgen::LocationError{});
            cout << futils::wrap::pack(d.out(), cout.is_tty() ? "\n" : "");
        }
        else {
            print_ok();
        }
        return exit_ok;
    }

    if (!cap.parser) {
        print_error("parser mode is disabled");
        return exit_err;
    }
    brgen::LocationError err_or_warn;
    std::shared_ptr<brgen::ast::Program> res;
    auto code = parse_and_analyze(&res, files, input, flags, cap, err_or_warn);

    if (code != exit_ok) {
        return code;
    }

    if (cout.is_tty() && !flags.print_json) {
        print_ok();
        return exit_ok;
    }

    if (!cap.ast_json) {
        print_error("stdout is disabled");
        return exit_err;
    }

    brgen::JSONWriter d;
    d.set_no_colon_space(true);
    if (flags.debug_json) {
        d = dump_json_file(files, true, res, "ast", std::move(err_or_warn));
    }
    else {
        brgen::ast::JSONConverter c;
        c.obj.set_no_colon_space(true);
        c.encode(res);
        d = dump_json_file(files, true, c.obj, "ast", std::move(err_or_warn));
    }
    cout << futils::wrap::pack(d.out(), cout.is_tty() ? "\n" : "");

    return exit_ok;
}

int src2json_main_noexcept(int argc, char** argv, const Capability& cap) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
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
                print_error(str.substr(0, str.size() - 1));
                print_note("use --help for help");
            }
            else {
                cout << str;
            }
        },
        [&](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx, cap);
        },
        true);
}

// entry point of src2json
int src2json_main(int argc, char** argv, const Capability& cap) {
    try {
        return src2json_main_noexcept(argc, argv, cap);
    } catch (const std::exception& e) {
        print_error("uncaught exception: ", e.what());
        return exit_err;
    } catch (...) {
        print_error("uncaught exception");
        return exit_err;
    }
}
