/*license*/
#pragma once
#include <core/middle/typing.h>
#include <core/middle/replace_assert.h>

namespace brgen::middle::test {
    result<void> apply_middle(LocationError& warn, std::shared_ptr<ast::Program>& node) {
        middle::replace_assert(node);
        return middle::analyze_type(node, &warn).and_then([&] {
            brgen::middle::collect_unused_warnings(node, warn);
            return result<void>{};
        });
    }
}  // namespace brgen::middle::test
