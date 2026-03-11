/*license*/
#include "bm2cpp.hpp"
#include <bm2/entry.hpp>
struct Flags : bm2::Flags {
};

DEFINE_ENTRY(Flags, bm2::Output) {
    bm2cpp::to_cpp(w, bm, output);
    return 0;
}
