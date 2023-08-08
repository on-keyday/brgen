/*license*/
#pragma once
#include "../ast/ast.h"
#include "../ast/translated.h"
#include <helper/defer.h>

namespace treeopt {
    struct ExtractContext {
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

}  // namespace treeopt