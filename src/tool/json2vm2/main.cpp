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
    bool jit = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
        ctx.VarBool(&run, "r", "run mode");
        ctx.VarBool(&hex, "x,hex", "hex text input for binary input (ignore spaces and newlines, # is comment)");
        ctx.VarString<true>(&call, "c,call", "call function in run mode", "<function name>");
        ctx.VarString<true>(&binary_input, "b,binary", "binary input", "<file or - (stdin)>");
        ctx.VarBool(&legacy_file_pass, "f,file", "use legacy file pass mode");
        ctx.VarBool(&jit, "jit", "enable jit compile mode");
    }
};

int run(const Flags& flags, futils::view::rvec code) {
    futils::file::FileStream<std::string> fs{futils::file::File::stdin_file()};
    using HexFilter = futils::number::hex::HexFilter<std::string, futils::byte, futils::binary::reader>;
    futils::file::View input;
    std::unique_ptr<HexFilter> hex_filter;
    futils::binary::bit_reader input_reader{futils::view::rvec{}};
    if (!flags.binary_input.empty()) {
        if (flags.binary_input == "-") {
            input_reader = futils::binary::reader(fs.get_read_handler(), &fs);
        }
        else {
            if (!input.open(flags.binary_input) || !input.data()) {
                print_error("cannot open file ", flags.binary_input);
                return 1;
            }
            input_reader = futils::binary::reader(input);
        }
        if (flags.hex) {
            hex_filter = std::make_unique<HexFilter>(HexFilter{std::move(input_reader.get_base())});
            input_reader = futils::binary::reader(hex_filter->get_read_handler(), hex_filter.get());
        }
    }
    std::string memory;
    memory.resize(1024 * 1024);  // 1MB
    brgen::vm2::VM2 vm;
    vm.reset(code, memory, 1024);
    if (flags.jit) {
        auto compiled = vm.jit_compile();
        if (!compiled.valid()) {
            print_error("cannot compile code");
            return 1;
        }
        auto f = compiled.as_function<std::uintptr_t, std::uintptr_t>();
        f(0);
        f(0);
        return 0;
    }
    vm.resume();
    while (vm.handle_syscall(&input_reader)) {
        vm.resume();
    }
    return 0;
}

int vm_generate(const Flags& flags, brgen::request::GenerateSource& req, std::shared_ptr<brgen::ast::Node> res) {
    auto prog = brgen::ast::cast_to<brgen::ast::Program>(res);
    std::string buffer;
    futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
    brgen::vm2::compile(prog, w);
    if (flags.run) {
        if (!flags.legacy_file_pass) {
            send_error_and_end(req.id, "run mode is not supported in stdin_stream mode");
            return 1;
        }
        return run(flags, w.written());
    }
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
