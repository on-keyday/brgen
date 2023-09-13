/*license*/
#pragma once
#include <core/ast/node/traverse.h>

namespace brgen::middle {

    inline void replace_assert(const std::shared_ptr<ast::Node>& node) {
        if (!node) {
            return;
        }
        auto each_element = [&](ast::node_list& list) {
            for (auto it = list.begin(); it != list.end(); it++) {
                replace_assert(*it);
                if (auto bin = ast::as<ast::Binary>(*it); bin && ast::is_boolean_op(bin->op)) {
                    *it = std::make_shared<ast::Assert>(std::static_pointer_cast<ast::Binary>(*it));
                }
            }
        };
        if (auto a = ast::as<ast::Program>(node)) {
            each_element(a->elements);
            return;
        }
        if (auto b = ast::as<ast::IndentScope>(node)) {
            each_element(b->elements);
            return;
        }
        ast::traverse(node, [](auto&& f) {
            replace_assert(f);
        });
    }
}  // namespace brgen::middle
