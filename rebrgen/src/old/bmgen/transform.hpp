/*license*/
#pragma once
#include "internal.hpp"

namespace rebgn {
    Error flatten(Module& m);
    Error decide_bit_field_size(Module& m);
    Error bind_encoder_and_decoder(Module& m);
    Error sort_formats(Module& m, const std::shared_ptr<ast::Node>& node);
    Error merge_conditional_field(Module& m);
    Error derive_property_functions(Module& m);
    Error generate_cfg1(Module& m);
    Error expand_bit_operation(Module& m);
    void replace_call_encode_decode_ref(Module& m);
    Error apply_encode_fallback(Module& m);
    Error apply_dynamic_endian_fallback(Module& m);
    Error sort_func_in_format(Module& m);
    Error derive_bit_field_accessor_functions(Module& m);
}  // namespace rebgn
