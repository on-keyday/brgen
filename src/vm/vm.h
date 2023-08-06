/*license*/
#pragma once
#include "../ast/ast.h"
#include <helper/defer.h>

namespace vm {
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

        std::shared_ptr<ast::TmpVar> add_call(std::shared_ptr<ast::Call>&& c) {
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
        if constexpr (std::is_same_v<T, ast::Program>) {
            auto s = h.enter_holder(&c->program);
            for (auto it = c->program.begin(); it != c->program.end(); it++) {
                if (it->get()->type == ast::ObjectType::call) {
                    // simple call, needless to extract
                    continue;
                }
                if (auto bin = ast::as<ast::Binary>(*it);
                    bin && bin->op == ast::BinaryOp::assign &&
                    bin->right->type == ast::ObjectType::call) {
                    // simple call with simple assignment, needless to extract
                    continue;
                }
                extract_call(h, *it);
            }
        }
        else if constexpr (std::is_same_v<T, ast::IndentScope>) {
            auto s = h.enter_holder(&c->elements);
            for (auto it = c->elements.begin(); it != c->elements.end(); it++) {
                if (it->get()->type == ast::ObjectType::call) {
                    // simple call, needless to extract
                    continue;
                }
                if (auto bin = ast::as<ast::Binary>(*it);
                    bin && bin->op == ast::BinaryOp::assign &&
                    bin->right->type == ast::ObjectType::call) {
                    // simple call with simple assignment, needless to extract
                    continue;
                }
                extract_call(h, *it);
            }
        }
        else {
            ast::traverse(c, [&](auto&& f) {
                extract_call(h, f);
            });
            if constexpr (std::is_base_of_v<T, ast::Call>) {
                if (ast::as<ast::Call>(c)) {
                    c = h.add_call(std::static_pointer_cast<ast::Call>(c));
                    return;
                }
            }
        }
    }
}  // namespace vm
