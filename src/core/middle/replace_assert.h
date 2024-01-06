/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>

namespace brgen::middle {

    inline void replace_assert(LocationError& err, const std::shared_ptr<ast::Node>& node) {
        if (!node) {
            return;
        }
        auto one_element = [&](auto it) {
            replace_assert(err, *it);
            if (auto bin = ast::as<ast::Binary>(*it); bin && ast::is_boolean_op(bin->op)) {
                *it = std::make_shared<ast::Assert>(std::static_pointer_cast<ast::Binary>(*it));
            }
            else if (bin && ast::is_assign_op(bin->op)) {
                // nothing to do
            }
            else if (bin || ast::as<ast::Literal>(*it) || ast::as<ast::Unary>(*it) || ast::as<ast::Ident>(*it)) {
                err.warning((*it)->loc, "unused expression");
            }
        };
        auto each_element = [&](ast::node_list& list) {
            for (auto it = list.begin(); it != list.end(); it++) {
                one_element(it);
            }
        };
        if (auto a = ast::as<ast::Program>(node)) {
            each_element(a->elements);
            return;
        }
        if (auto b = ast::as<ast::IndentBlock>(node)) {
            each_element(b->elements);
            return;
        }
        if (auto s = ast::as<ast::ScopedStatement>(node)) {
            one_element(&s->statement);
            return;
        }
        ast::traverse(node, [&](auto&& f) {
            replace_assert(err, f);
        });
    }
}  // namespace brgen::middle
