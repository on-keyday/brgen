/*license*/
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/converter.hpp"
#include "../convert/helper.hpp"
#include "transform.hpp"
#include <cstddef>
#include <testutil/timer.h>

namespace ebmgen {
    expected<void> flatten_io_expression(TransformContext& tctx) {
        auto& ctx = tctx.context();
        auto& all_statements = tctx.statement_repository().get_all();
        size_t current_index = all_statements.size();
        for (size_t i = 0; i < current_index; i++) {
            auto block = get_block(all_statements[i].body);
            if (!block) {
                continue;
            }
            for (size_t j = 0; j < block->container.size(); j++) {
                auto stmt_ref = block->container[j];
                MAYBE(stmt, tctx.statement_repository().get(stmt_ref));
                if (auto io_expr = stmt.body.expression()) {
                    MAYBE(expr, tctx.expression_repository().get(*io_expr));
                    if (expr.body.kind == ebm::ExpressionKind::READ_DATA) {
                        MAYBE(io_def, expr.body.target_stmt());
                        MAYBE(io_stmt, expr.body.io_statement());
                        ebm::Block new_block;
                        append(new_block, io_def);
                        append(new_block, io_stmt);
                        EBM_BLOCK(new_body, std::move(new_block));
                        block = get_block(all_statements[i].body);  // refresh block pointer
                        block->container[j] = new_body;
                    }
                }
            }
        }
        return {};
    }
}  // namespace ebmgen
