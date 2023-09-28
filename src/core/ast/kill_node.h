/*license*/
#pragma once

#include "traverse.h"

namespace brgen::ast {
    // for large scale of AST
    // destructor call stack is too deep
    // so we need to use this
    void kill_node(std::shared_ptr<Node>& node) {
        std::vector<std::shared_ptr<Node>> stack;
        stack.push_back(node);
        node = nullptr;
        while (!stack.empty()) {
            auto n = std::move(stack.back());
            stack.pop_back();
            traverse(n, [&](auto& v) {
                stack.push_back(std::move(v));
                v = nullptr;
            });
        }
    }
}  // namespace brgen::ast
