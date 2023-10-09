/*license*/
#include <console/ansiesc.h>
#include <wrap/cout.h>
inline auto& cerr = utils::wrap::cerr_wrap();
namespace cse = utils::console::escape;

inline const char*& prefix_loc() {
    static const char* prefix = "src2json: ";
    return prefix;
}

bool no_color = false;

void print_error(auto&&... msg) {
    auto p = utils::wrap::pack();
    p << prefix_loc();
    if (!no_color && cerr.is_tty()) {
        p << cse::letter_color_code<9>;
    }
    p << "error: ";
    if (!no_color && cerr.is_tty()) {
        p << cse::letter_color<cse::ColorPalette::white>;
    }
    (p << ... << msg);
    if (!no_color && cerr.is_tty()) {
        p << "\n"
          << cse::color_reset;
    }
    cerr << p.pack();
}

void print_warning(auto&&... msg) {
    auto p = utils::wrap::pack();
    p << prefix_loc();
    if (!no_color && cerr.is_tty()) {
        p << cse::letter_color<cse::ColorPalette::yellow>;
    }
    p << "warning: ";
    if (!no_color && cerr.is_tty()) {
        p << cse::letter_color<cse::ColorPalette::white>;
    }
    (p << ... << msg);
    if (!no_color && cerr.is_tty()) {
        p << "\n"
          << cse::color_reset;
    }
    cerr << p.pack();
}
