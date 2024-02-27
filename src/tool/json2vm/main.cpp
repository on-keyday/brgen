/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <vm/compile.h>
#include "../common/print.h"
#include <file/file_view.h>
#include <core/ast/json.h>
#include <core/ast/file.h>
#include <wrap/cin.h>
#include "hex.h"

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool run = false;
    bool hex = false;
    std::string_view call;
    std::string_view binary_input;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&run, "r", "run mode");
        ctx.VarBool(&hex, "x,hex", "hex text input for binary input (ignore spaces and newlines, # is comment)");
        ctx.VarString<true>(&call, "c,call", "call function in run mode", "<function name>");
        ctx.VarString<true>(&binary_input, "b,binary", "binary input", "<file or - (stdin)>");
    }
};

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.spec) {
        cout << R"({
            "input": "file",
            "langs": ["vm"],
            "suffix": [".bvm"],
            "separator": "############\n"
        })";
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
    auto code = brgen::vm::compile(brgen::ast::cast_to<brgen::ast::Program>(*res));
    if (flags.run) {
        brgen::vm::VM vm;
        vm.set_inject([](brgen::vm::VM& vm, const brgen::vm::Instruction& instr, size_t& pc) {
            cout << pc << ": " << brgen::vm::to_string(instr.op()) << " " << brgen::nums(instr.arg()) << "\n";
        });
        futils::file::View input;
        std::string input_buf;
        if (!flags.binary_input.empty()) {
            if (flags.binary_input == "-") {
                auto res = futils::file::File::stdin_file().read_all([&](futils::view::wvec buf) {
                    input_buf.append(buf.as_char(), buf.size());
                    return true;
                });
                if (!res) {
                    std::string err_msg;
                    res.error().error(err_msg);
                    print_error("cannot read stdin: ", err_msg);
                    return 1;
                }
                vm.set_input(input_buf);
            }
            else {
                if (!input.open(flags.binary_input)) {
                    print_error("cannot open file ", flags.binary_input);
                    return 1;
                }
                vm.set_input(input);
            }
            if (flags.hex) {
                auto seq = futils::make_cpy_seq(vm.get_input());
                auto h = json2vm::hex::read_hex(seq);
                if (!h) {
                    print_error("cannot read hex input");
                    return 1;
                }
                input_buf = std::move(*h);
                vm.set_input(input_buf);
            }
        }
        vm.execute(code);
        if (!vm.error().empty()) {
            print_error("vm error: ", vm.error());
            return 1;
        }
        if (!flags.call.empty()) {
            // set this to an empty object
            vm.execute(brgen::vm::Instruction{brgen::vm::Op::MAKE_OBJECT, brgen::vm::this_register});
            vm.call(code, flags.call.data());
            if (!vm.error().empty()) {
                print_error("vm error: ", vm.error());
                return 1;
            }
            auto val = vm.get_register(brgen::vm::this_register);
            std::string buf;
            brgen::vm::print_value(buf, val);
            cout << buf << "\n";
        }
    }
    std::string buf;
    brgen::vm::print_code(buf, code);
    cout << buf;
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
