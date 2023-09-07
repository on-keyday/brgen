/*license*/
#pragma once
#include "extract_context.h"
#include <core/ast/node/traverse.h>
#include <helper/defer.h>

namespace brgen::treeopt {

    template <class T>
    void extract_call(ExtractContext& h, std::shared_ptr<T>& c) {
        if (!c) {
            return;
        }

        auto direct_call = [&](auto& c) {
            if (auto call = ast::as<ast::Call>(c)) {
                extract_call(h, call->expr_type);
                extract_call(h, call->callee);
                extract_call(h, call->arguments);
                return true;
            }
            if (auto b = ast::as<ast::Binary>(c);
                b && b->op == ast::BinaryOp::assign &&
                b->right->type == ast::NodeType::call) {
                extract_call(h, b->left);
                auto call = ast::as<ast::Call>(b->right);
                extract_call(h, call->expr_type);
                extract_call(h, call->callee);
                extract_call(h, call->arguments);
                return true;
            }
            if (auto if_ = ast::as<ast::If>(c)) {
                ast::node_list l;
                {
                    auto scope = h.enter_holder(&l);
                    extract_call(h, if_->cond);
                    if (l.size()) {
                        if_->cond = std::make_shared<ast::BlockExpr>(std::move(if_->cond), std::move(l));
                    }
                }
                extract_call(h, if_->block);
                extract_call(h, if_->els);
                return true;
            }
            return false;
        };

        if constexpr (std::is_same_v<T, ast::Program> || std::is_same_v<T, ast::IndentScope>) {
            auto s = h.enter_holder(&c->elements);
            for (auto it = c->elements.begin(); it != c->elements.end(); it++) {
                if (!direct_call(*it)) {
                    extract_call(h, *it);
                }
            }
        }
        else {
            if constexpr (std::is_base_of_v<T, ast::If>) {
                if (ast::If* if_ = ast::as<ast::If>(c)) {
                    ast::node_list l;
                    {
                        auto scope = h.enter_holder(&l);
                        extract_call(h, if_->cond);
                        if (l.size()) {
                            if_->cond = std::make_shared<ast::BlockExpr>(std::move(if_->cond), std::move(l));
                        }
                    }
                    extract_call(h, if_->block);
                    extract_call(h, if_->els);
                    c = h.add_tmp_var(std::static_pointer_cast<ast::If>(c));
                    return;
                }
            }

            ast::traverse(c, [&](auto&& f) {
                extract_call(h, f);
            });

            if constexpr (std::is_base_of_v<T, ast::Call>) {
                if (ast::as<ast::Call>(c)) {
                    c = h.add_tmp_var(std::static_pointer_cast<ast::Call>(c));
                    return;
                }
            }
        }
    }
}  // namespace brgen::treeopt
