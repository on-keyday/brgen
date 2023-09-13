/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <core/ast/parse.h>
#include <core/ast/json.h>
#include <wrap/iocommon.h>
#include <console/ansiesc.h>

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
};
auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();
namespace cse = utils::console::escape;
void print_error(auto&&... msg) {
    if (cerr.is_tty()) {
        cerr << cse::letter_color<cse::ColorPalette::red>;
    }
    cerr << "error: ";
    if (cerr.is_tty()) {
        cerr << cse::letter_color<cse::ColorPalette::white>;
    }
    (cerr << ... << msg) << "\n";
    if (cerr.is_tty()) {
        cerr << cse::color_reset;
    }
}

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    utils::wrap::out_virtual_terminal = true;

    if (flags.args.size() == 0) {
        print_error("no input file");
        return -1;
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
