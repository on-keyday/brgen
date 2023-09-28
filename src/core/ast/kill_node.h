/*license*/
#pragma once

#include "traverse.h"

namespace brgen::ast {
    // for large scale of AST
    // destructor call stack is too deep
    // so we need to use this
    void kill_node(const std::shared_ptr<Node>& node) {
        if (auto p = ast::as<Program>(node); p && p->global_scope) {
            std::vector<std::shared_ptr<Scope>> stack;
            stack.push_back(std::move(p->global_scope));
            p->global_scope = nullptr;
            while (!stack.empty()) {
                auto s = std::move(stack.back());
                stack.pop_back();
                if (s->next) {
                    stack.push_back(std::move(s->next));
                    s->next = nullptr;
                }
                if (s->branch) {
                    stack.push_back(std::move(s->branch));
                    s->branch = nullptr;
                }
            }
        }
        std::vector<std::shared_ptr<Node>> stack;
        stack.push_back(node);
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
