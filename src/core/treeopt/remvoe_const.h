/*license*/
#pragma once
#include "core/ast/ast.h"
#include "core/ast/translated.h"
#include "core/ast/traverse.h"

namespace brgen::treeopt {
    inline bool is_const(const std::shared_ptr<ast::Expr>& expr) {}

    inline void remove_const(const std::shared_ptr<ast::Node>& node) {
        if (!node) {
            return;
        }
        auto each_element = [&](ast::node_list& list) {
            for (auto it = list.begin(); it != list.end(); it++) {
                remove_const(*it);
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
}  // namespace brgen::treeopt