/*license*/
#pragma once
#include <core/ast/node/traverse.h>
#include "core/common/error.h"

namespace brgen::treeopt {
    inline bool is_const(const std::shared_ptr<ast::Expr>& expr) {
        if (auto bin = ast::as<ast::Binary>(expr)) {
            return is_const(bin->left) &&
                   is_const(bin->right);
        }
        if (auto u = ast::as<ast::Unary>(expr)) {
            return is_const(u->target);
        }
        if (auto p = ast::as<ast::Paren>(expr)) {
            return is_const(p->expr);
        }
        if (auto i = ast::as<ast::IntLiteral>(expr)) {
            return true;
        }
        if (auto ident = ast::as<ast::Ident>(expr)) {
            return true;
        }
        return false;
    }

    inline void remove_const(LocationError& warn, const std::shared_ptr<ast::Node>& node) {
        if (!node) {
            return;
        }
        auto each_element = [&](ast::node_list& list) {
            for (auto it = list.begin(); it != list.end();) {
                remove_const(warn, *it);
                if (ast::as<ast::Expr>(*it)) {
                    if (is_const(std::static_pointer_cast<ast::Expr>(*it))) {
                        warn.warning((*it)->loc, "removing unused constant value; use ==,!=,<,<=,>,>=,&& or || for assertion");
                        it = list.erase(it);
                        continue;
                    }
                }
                it++;
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
