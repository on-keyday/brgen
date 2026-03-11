/*license*/
#include "transform.hpp"
#include "../common.hpp"
#include "control_flow_graph.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/converter.hpp"
#include <testutil/timer.h>

namespace ebmgen {

    expected<void> transform(TransformContext& ctx, bool debug, std::function<void(const char*)> timer) {
        MAYBE_VOID(flatten_io_expression, flatten_io_expression(ctx));
        if (timer) {
            timer("flatten io expression");
        }
        // internal CFG used optimization
        {
            CFGContext cfg_ctx{ctx};
            MAYBE(cfg, analyze_control_flow_graph(cfg_ctx.stack, {&ctx.context().repository(), &ctx.statement_repository().get_all()}));
            if (timer) {
                timer("initial cfg");
            }
            MAYBE_VOID(bit_io_read, lowered_dynamic_bit_io(cfg_ctx, false));
            if (timer) {
                timer("bit io read");
            }
            MAYBE_VOID(bit_io_write, lowered_dynamic_bit_io(cfg_ctx, true));
            if (timer) {
                timer("bit io write");
            }
        }
        MAYBE_VOID(merge_bit_field, merge_bit_field(ctx));
        if (timer) {
            timer("merge bit field");
        }
        MAYBE_VOID(vio_read, vectorized_io(ctx, false));
        if (timer) {
            timer("vectorized io read");
        }
        MAYBE_VOID(vio_write, vectorized_io(ctx, true));
        if (timer) {
            timer("vectorized io write");
        }
        MAYBE_VOID(prop_setter_getter, derive_property_setter_getter(ctx));
        if (timer) {
            timer("derive property setter/getter");
        }
        MAYBE_VOID(add_cast, add_cast_func(ctx));
        if (timer) {
            timer("add cast function");
        }
        MAYBE_VOID(array_setter, derive_array_setter(ctx));
        if (timer) {
            timer("derive array setter");
        }
        if (!debug) {
            MAYBE_VOID(remove_unused, remove_unused_object(ctx, timer));
            ctx.recalculate_id_index_map();
        }
        return {};
    }
}  // namespace ebmgen
