/*license*/
#pragma once
#include "core/typing/typing.h"
#include "core/treeopt/replace_assert.h"
#include "core/treeopt/extract_call.h"
#include "core/treeopt/remvoe_const.h"

namespace brgen::middle {
    result<void> apply_middle(LocationError& warn, std::shared_ptr<ast::Program>& node) {
        return typing::Typing{}.typing(node).and_then([&] {
            treeopt::replace_assert(node);
            treeopt::remove_const(warn, node);
            treeopt::ExtractContext ctx;
            // treeopt::extract_call(ctx, node);
            return result<void>{};
        });
    }
}  // namespace brgen::middle
