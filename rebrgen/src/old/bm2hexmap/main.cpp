/*license*/
#include "bm2hexmap.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
    bm2hexmap::Flags bm2hexmap_flags;
    std::string binary_file;
    void bind(futils::cmdline::option::Context& ctx) {
        bm2::Flags::bind(ctx);
        ctx.VarString<true>(&binary_file, "binary", "binary file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&bm2hexmap_flags.start_format_name, "start-format", "start format name", "NAME", futils::cmdline::option::CustomFlag::required);
    }
};
DEFINE_ENTRY(Flags, bm2::Output) {
    futils::file::View view;
    if (auto res = view.open(flags.binary_file); !res) {
        futils::wrap::cerr_wrap() << res.error().template error<std::string>() << '\n';
        return 1;
    }
    if (!view.data()) {
        futils::wrap::cerr_wrap() << "Empty file\n";
        return 1;
    }
    flags.bm2hexmap_flags.input_binary = view;
    bm2hexmap::to_hexmap(w, bm, flags.bm2hexmap_flags, output);
    return 0;
}
