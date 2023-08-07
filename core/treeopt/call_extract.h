/*license*/
#pragma once
#include "../ast/ast.h"
#include "../ast/translated.h"
#include "../ast/util.h"
#include <helper/defer.h>

namespace treeopt {
    struct CallHolder {
       private:
        size_t tmp_index = 0;
        ast::objlist* holder = nullptr;
        using iterator = typename ast::objlist::iterator;
        iterator cur;

       public:
        auto enter_holder(ast::objlist* l) {
            auto old_holder = holder;
            auto old_cur = std::move(cur);
            holder = l;
            cur = l->begin();
            return utils::helper::defer([=, this] {
                holder = old_holder;
                cur = std::move(old_cur);
            });
        }

        std::shared_ptr<ast::TmpVar> add_tmp_var(std::shared_ptr<ast::Expr>&& c) {
            auto t = c;
            auto tmp = std::make_shared<ast::TmpVar>(std::move(t), tmp_index);
            tmp_index++;
            auto obj = tmp;
            auto assign = std::make_shared<ast::Binary>(c->loc, std::move(obj), ast::BinaryOp::assign);
            assign->right = std::move(c);
            holder->insert(cur, assign);
            return tmp;
        }
    };

    template <class T>
    void extract_call(CallHolder& h, std::shared_ptr<T>& c) {
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
                b->right->type == ast::ObjectType::call) {
                extract_call(h, b->left);
                auto call = ast::as<ast::Call>(b->right);
                extract_call(h, call->expr_type);
                extract_call(h, call->callee);
                extract_call(h, call->arguments);
                return true;
            }
            return false;
        };
        if constexpr (std::is_same_v<T, ast::Program> || std::is_same_v<T, ast::IndentScope>) {
            auto s = h.enter_holder(&c->elements);
            for (auto it = c->elements.begin(); it != c->elements.end(); it++) {
                if (direct_call(*it)) {
                    continue;
                }
                extract_call(h, *it);
            }
        }
        else {
            if constexpr (std::is_base_of_v<T, ast::If>) {
                if (ast::If* if_ = ast::as<ast::If>(c)) {
                    ast::objlist l;
                    {
                        auto scope = h.enter_holder(&l);
                        extract_call(h, if_->cond);
                        if (l.size()) {
                            if_->cond = std::make_shared<ast::BlockExpr>(std::move(if_->cond), std::move(l));
                        }
                    }
                    extract_call(h, if_->block);
                    extract_call(h, if_->els);
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
}  // namespace treeopt
