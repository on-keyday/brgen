/*license*/
#include <console/ansiesc.h>
#include <wrap/cout.h>
inline auto& cerr = utils::wrap::cerr_wrap();
namespace cse = utils::console::escape;

inline const char*& prefix_loc() {
    static const char* prefix = "src2json: ";
    return prefix;
}

enum class ColorMode {
    auto_color,
    force_color,
    no_color,
};

ColorMode cerr_color_mode = ColorMode::auto_color;

void print_error(auto&&... msg) {
    assert(cerr_color_mode != ColorMode::auto_color);
    auto p = utils::wrap::pack();
    p << prefix_loc();
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::letter_color_code<9>;
    }
    p << "error: ";
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::letter_color<cse::ColorPalette::white>;
    }
    (p << ... << msg) << "\n";
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::color_reset;
    }
    cerr << p.pack();
}

void print_warning(auto&&... msg) {
    assert(cerr_color_mode != ColorMode::auto_color);
    auto p = utils::wrap::pack();
    p << prefix_loc();
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::letter_color<cse::ColorPalette::yellow>;
    }
    p << "warning: ";
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::letter_color<cse::ColorPalette::white>;
    }
    (p << ... << msg) << "\n";
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::color_reset;
    }
    cerr << p.pack();
}

void print_note(auto&&... msg) {
    assert(cerr_color_mode != ColorMode::auto_color);
    auto p = utils::wrap::pack();
    p << prefix_loc();
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::letter_color<cse::ColorPalette::cyan>;
    }
    p << "note: ";
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::letter_color<cse::ColorPalette::white>;
    }
    (p << ... << msg) << "\n";
    if (cerr_color_mode == ColorMode::force_color) {
        p << cse::color_reset;
    }
    cerr << p.pack();
}