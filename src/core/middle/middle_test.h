/*license*/
#pragma once
#include "typing.h"
#include "replace_assert.h"

namespace brgen::middle::test {
    result<void> apply_middle(LocationError& warn, std::shared_ptr<ast::Program>& node) {
        return Typing{}.typing(node).and_then([&] {
            middle::replace_assert(warn, node);
            return result<void>{};
        });
    }
}  // namespace brgen::middle::test
