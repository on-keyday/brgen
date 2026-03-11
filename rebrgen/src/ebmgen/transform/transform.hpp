/*license*/
#pragma once
#include "../converter.hpp"
#include "control_flow_graph.hpp"
#include "ebmgen/common.hpp"

namespace ebmgen {

    expected<void> transform(TransformContext& ctx, bool debug, std::function<void(const char*)> timer);

    ebm::Block* get_block(ebm::StatementBody& body);
    expected<void> vectorized_io(TransformContext& tctx, bool write);
    expected<void> remove_unused_object(TransformContext& ctx, std::function<void(const char*)> timer);
    expected<void> lowered_dynamic_bit_io(CFGContext& tctx, bool write);
    expected<void> merge_bit_field(TransformContext& tctx);
    expected<void> derive_property_setter_getter(TransformContext& tctx);
    expected<void> flatten_io_expression(TransformContext& tctx);
    expected<void> add_cast_func(TransformContext& tctx);
    expected<void> derive_array_setter(TransformContext& tctx);
}  // namespace ebmgen
