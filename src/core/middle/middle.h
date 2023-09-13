/*license*/
#pragma once
#include "typing.h"
#include "replace_assert.h"
#include "extract_call.h"
#include "remvoe_const.h"

namespace brgen::middle {
    result<void> apply_middle(LocationError& warn, std::shared_ptr<ast::Program>& node) {
        return Typing{}.typing(node).and_then([&] {
            middle::replace_assert(node);
            middle::remove_const(warn, node);
            middle::ExtractContext ctx;
            // middle::extract_call(ctx, node);
            return result<void>{};
        });
    }
}  // namespace brgen::middle
