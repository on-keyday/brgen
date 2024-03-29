/*license*/
#pragma once
#include <console/ansiesc.h>
#include <wrap/cout.h>
#ifdef SRC2JSON_DLL
inline futils::wrap::UtfOut cout{futils::file::File::stdout_file()};
inline futils::wrap::UtfOut cerr{futils::file::File::stderr_file()};
#else
inline auto& cout = futils::wrap::cout_wrap();
inline auto& cerr = futils::wrap::cerr_wrap();
#endif

namespace cse = futils::console::escape;

inline const char*& prefix_loc() {
    static const char* prefix = "src2json: ";
    return prefix;
}

enum class ColorMode {
    auto_color,
    force_color,
    no_color,
};

inline thread_local ColorMode cerr_color_mode = ColorMode::auto_color;

inline void print_error(auto&&... msg) {
    assert(cerr_color_mode != ColorMode::auto_color);
    auto p = futils::wrap::pack();
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

inline void print_warning(auto&&... msg) {
    assert(cerr_color_mode != ColorMode::auto_color);
    auto p = futils::wrap::pack();
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

inline void print_note(auto&&... msg) {
    assert(cerr_color_mode != ColorMode::auto_color);
    auto p = futils::wrap::pack();
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