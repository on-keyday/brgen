/*license*/
#pragma once

#include <bm/binary_module.hpp>
#include <bm2/output.hpp>

namespace bm2cpp {
    void to_cpp(futils::binary::writer& w, const rebgn::BinaryModule& bm, bm2::Output& output);
}  // namespace bm2cpp
