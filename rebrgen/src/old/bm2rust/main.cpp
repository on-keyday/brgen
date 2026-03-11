/*license*/
#include <bm2/entry.hpp>
#include "bm2rust.hpp"
struct Flags : bm2::Flags {
    bm2rust::Flags bm2rust_flags;

    void bind(futils::cmdline::option::Context& ctx) {
        bm2::Flags::bind(ctx);
        ctx.VarBool(&bm2rust_flags.use_async, "async", "enable async");
        ctx.VarBool(&bm2rust_flags.use_copy_on_write_vec, "copy-on-write", "use copy-on-write vectors");
    }
};

DEFINE_ENTRY(Flags, bm2::Output) {
    bm2rust::to_rust(w, bm, flags.bm2rust_flags, output);
    return 0;
}
