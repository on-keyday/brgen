/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include "../common/print.h"
#include <core/ast/json.h>
#include <json/convert_json.h>
#include <core/ast/file.h>
#include <file/file_view.h>
#include "generate.h"
#include <wrap/argv.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../common/em_main.h"
#endif
#include <request/stream.hpp>
#include <filesystem>

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

void send_response(const brgen::request::SourceCode& code) {
    futils::file::FileStream<std::string> fs{cout.get_file()};
    futils::binary::writer w{fs.get_write_handler(), &fs};
    auto prev = w.offset();
    if (auto err = code.encode(w)) {
        assert(false);  // should not happen
        w.reset(prev);  // reset for consistency
        return;         // ignore error
    }
    // commit by writer handler
}

thread_local bool text = false;

void send_error(std::uint64_t id, auto&&... msg) {
    if (text) {
        print_error(msg...);
        return;
    }
    auto err_msg = brgen::concat(msg...);
    brgen::request::SourceCode code;
    code.id = id;
    code.status = brgen::request::ResponseStatus::ERROR;
    code.set_error_message(err_msg);
    send_response(code);
}

void send_source(std::uint64_t id, std::string&& src, const auto&& name) {
    if (text) {
        cout << src << "\n";
        return;
    }
    brgen::request::SourceCode code;
    code.id = id;
    code.status = brgen::request::ResponseStatus::OK;
    code.set_code(std::move(src));
    code.set_name(name);
    send_response(code);
}

template <class T>
int do_generate(Flags& flags, brgen::request::GenerateSource& req, T&& input) {
    auto js = futils::json::parse<futils::json::JSON>(input);
    if (js.is_undef()) {
        send_error(req.id, "cannot parse json file: ", req.name);
        return 1;
    }
    brgen::ast::AstFile file;
    if (!futils::json::convert_from_json(js, file)) {
        send_error(req.id, "cannot convert json file to ast: ", req.name);
        return 1;
    }
    if (!file.ast) {
        send_error(req.id, "cannot convert json file to ast: ast is null: ", req.name);
        return 1;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*file.ast);
    if (!res) {
        send_error(req.id, "cannot decode json file: ", res.error().locations[0].msg);
        return 1;
    }
    if (!*res) {
        send_error(req.id, "cannot decode json file: ast is null: ", req.name);
        return 1;
    }
    j2cp2::Generator g;
    g.enable_line_map = flags.add_line_map;
    g.use_error = flags.use_error;
    g.use_variant = !flags.use_raw_union;
    g.use_overflow_check = flags.use_overflow_check;
    auto prog = brgen::ast::cast_to<brgen::ast::Program>(*res);
    g.write_program(prog);
    send_source(req.id, std::move(g.w.out()), req.name + ".hpp");
    if (flags.add_line_map) {
        if (text) {
            cout << "############\n";
        }
        futils::json::Stringer s;
        {
            auto field = s.object();
            field("structs", g.struct_names);
            field("line_map", g.line_map);
        }
        send_source(req.id, std::move(s.out()), req.name + ".hpp.map.json");
    }
    return 0;
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    prefix_loc() = "json2cpp: ";
    cerr_color_mode = flags.no_color ? ColorMode::no_color : cerr.is_tty() ? ColorMode::force_color
                                                                           : ColorMode::no_color;
    if (flags.spec) {
        cout << R"({
            "input": "file",
            "langs": ["cpp","json"],
            "suffix": [".hpp",".json"],
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
    std::filesystem::path path{name};
    auto u8name = path.filename().replace_extension().generic_u8string();
    brgen::request::GenerateSource req;
    req.id = 0;
    req.name = std::string(reinterpret_cast<const char*>(u8name.data()), u8name.size());
    text=true;
    return do_generate(flags, req, view);
}

int json2cpp_main(int argc, char** argv) {
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
    return em_main(cmdline, json2cpp_main);
}
#else
int main(int argc, char** argv) {
    return json2cpp_main(argc, argv);
}
#endif
