/*license*/
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/converter.hpp"
#include "../convert/helper.hpp"
#include "transform.hpp"

namespace ebmgen {

    static bool io_ref_has_absolute_offset(TransformContext& tctx, ebm::StatementRef io_ref) {
        auto stmt_res = tctx.statement_repository().get(io_ref);
        if (!stmt_res) {
            return false;
        }
        auto& stmt = *stmt_res;
        auto* param = stmt.body.param_decl();
        if (!param) {
            return false;
        }
        auto type_res = tctx.context().repository().get_type(param->param_type);
        if (!type_res) {
            return false;
        }
        auto& type = *type_res;
        auto* io_desc = type.body.io_input_desc();
        if (!io_desc) {
            return false;
        }
        return io_desc->has_absolute_offset();
    }

    static ebm::StatementBody make_update_offset(ebm::StatementRef io_ref, ebm::Size size) {
        /**
        ebm::StatementBody body;
        ebm::UpdateOffset update_offset;
        update_offset.io_ref = io_ref;
        update_offset.size = size;
        body.kind = ebm::StatementKind::UPDATE_OFFSET;
        body.update_offset(update_offset);
        return body;
        */
    }

    // TODO: This pass is currently disabled (not called from transform()).
    // Per-language abs_offset tracking is implemented directly in each language's
    // READ_DATA/WRITE_DATA/SUB_BYTE_RANGE visitors instead.
    expected<void> insert_update_offset(TransformContext& tctx) {
        auto& all_statements = tctx.statement_repository().get_all();
        size_t current_count = all_statements.size();
        for (size_t i = 0; i < current_count; i++) {
            auto block = get_block(all_statements[i].body);
            if (!block) {
                continue;
            }
            for (size_t j = 0; j < block->container.size(); j++) {
                auto stmt_ref = block->container[j];
                MAYBE(stmt, tctx.statement_repository().get(stmt_ref));
                ebm::IOData* io_data = nullptr;
                if (auto* rd = stmt.body.read_data()) {
                    io_data = rd;
                }
                else if (auto* wd = stmt.body.write_data()) {
                    io_data = wd;
                }
                if (!io_data) {
                    continue;
                }
                if (!io_ref_has_absolute_offset(tctx, io_data->io_ref)) {
                    continue;
                }
                // Only emit UPDATE_OFFSET for byte-unit sizes where the delta is directly
                // expressible in bytes. BIT_*, ELEMENT_*, DYNAMIC, and UNKNOWN are skipped:
                // bit-unit ops should have been merged into byte-level IO by earlier passes,
                // and element/dynamic ops require element-size knowledge to compute byte deltas.
                auto size_unit = io_data->size.unit;
                if (size_unit != ebm::SizeUnit::BYTE_FIXED &&
                    size_unit != ebm::SizeUnit::BYTE_DYNAMIC) {
                    continue;
                }
                auto& ctx = tctx.context();
                MAYBE(update_offset_ref, ctx.repository().add_statement(make_update_offset(io_data->io_ref, io_data->size)));
                ebm::Block new_block;
                append(new_block, stmt_ref);
                append(new_block, update_offset_ref);
                EBM_BLOCK(new_body, std::move(new_block));
                block = get_block(all_statements[i].body);  // refresh after potential realloc
                block->container[j] = new_body;
            }
        }
        return {};
    }

}  // namespace ebmgen
