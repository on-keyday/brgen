/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <wrap/cin.h>
#include "hex.h"
#include <view/concat.h>
#include <file/file_view.h>
struct Flags : futils::cmdline::templ::HelpOption {
    std::vector<std::string_view> args;
    bool bin2hex = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&bin2hex, "b", "binary to hex");
    }
};
auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();
auto& cin = futils::wrap::cin_wrap();

void append_to_buffer(std::string& buffer, size_t& size, futils::byte c) {
    buffer.push_back(futils::number::to_num_char(futils::byte(c) >> 4));
    buffer.push_back(futils::number::to_num_char(futils::byte(c) & 0xf));
    if (++size % 16 == 0) {
        buffer.push_back('\n');
        size = 0;
    }
    else {
        buffer.push_back(' ');
    }
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.args.empty()) {
        std::string buffer;
        size_t size = 0;
        auto err = cin.get_file().read_all([&](futils::view::rcvec data) {
            if (flags.bin2hex) {
                for (auto c : data) {
                    append_to_buffer(buffer, size, c);
                }
                cout.write(buffer);
                return true;
            }
            auto p = hex2bin::read_hex_stream(data, buffer);
            if (!p) {
                return false;
            }
            cout.write(*p);
            return true;
        });
        if (err) {
            std::string buf;
            err.error().error(buf);
            cerr << "hex2bin: error: " << buf << "\n";
            return 1;
        }
        if (buffer.size()) {
            cerr << "hex2bin: error: incomplete hex data\n";
            return 1;
        }
        return 0;
    }
    futils::file::View view;
    if (!view.open(flags.args[0])) {
        cerr << "error: cannot open file " << flags.args[0] << "\n";
        return 1;
    }
    if (flags.bin2hex) {
        size_t size = 0;
        std::string buffer;
        for (size_t i = 0; i < view.size(); i++) {
            append_to_buffer(buffer, size, view[i]);
        }
        cout.write(buffer);
        return 0;
    }
    auto p = hex2bin::read_hex(view);
    if (!p) {
        cerr << "error: cannot parse hex file " << flags.args[0] << "\n";
        return 1;
    }
    cout.write(*p);
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
