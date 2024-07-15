/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <vm/vm2/compile.h>
#include <vm/vm2/interpret.h>
#include "../common/print.h"
#include <file/file_view.h>
#include <core/ast/json.h>
#include <core/ast/file.h>
#include <wrap/cin.h>
#include <number/hex/hex2bin.h>
#include <file/file_stream.h>
#include "../common/generate.h"

struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    bool run = false;
    bool hex = false;
    std::string_view call;
    std::string_view binary_input;
    bool legacy_file_pass = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&run, "r", "run mode");
        ctx.VarBool(&hex, "x,hex", "hex text input for binary input (ignore spaces and newlines, # is comment)");
        ctx.VarString<true>(&call, "c,call", "call function in run mode", "<function name>");
        ctx.VarString<true>(&binary_input, "b,binary", "binary input", "<file or - (stdin)>");
        ctx.VarBool(&legacy_file_pass, "f,file", "use legacy file pass mode");
    }
};

int run(const Flags& flags) {
    return 0;
}

int vm_generate(const Flags& flags, brgen::request::GenerateSource& req, std::shared_ptr<brgen::ast::Node> res) {
    auto prog = brgen::ast::cast_to<brgen::ast::Program>(res);
    std::string buffer;
    futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
    brgen::vm2::compile(prog, w);
    if (flags.run) {
        std::string memory;
        memory.resize(1024 * 1024);  // 1MB
        brgen::vm2::VM2 vm;
        vm.reset(w.written(), memory, 1024);
        vm.resume();
        while (vm.handle_syscall()) {
            vm.resume();
        }
    }
    /*
    auto code = brgen::vm::compile(prog);
    if (flags.run) {
        if (!flags.legacy_file_pass) {
            send_error_and_end(req.id, "run mode is not supported in stdin_stream mode");
            return 1;
        }
        return run(flags, code);
    }
    std::string buf;
    brgen::vm::print_code(buf, code);
    */
    send_source_and_end(req.id, std::move(buffer), req.name + ".bvm");
    return 0;
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.spec) {
        if (flags.legacy_file_pass) {
            cout << R"({
            "input": "file",
            "langs": ["vm"],
            "suffix": [".bvm"],
            "separator": "############\n"
        })";
        }
        else {
            cout << R"({
            "input": "stdin_stream",
            "langs": ["vm"]
        })";
        }
        return 0;
    }
    cerr_color_mode = cerr.is_tty() ? ColorMode::force_color : ColorMode::no_color;
    if (flags.legacy_file_pass) {
        return generate_from_file(flags, vm_generate);
    }
    read_stdin_requests([&](brgen::request::GenerateSource& req) {
        do_generate(flags, req, req.json_text, vm_generate);
        return futils::error::Error<>{};
    });
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
