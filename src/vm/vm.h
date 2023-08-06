/*license*/
#pragma once
#include "../ast/ast.h"
#include <helper/defer.h>

namespace vm {
    struct CallHolder {
        ast::objlist* holder = nullptr;

        auto enter_holder(ast::objlist* l) {
            auto old = holder;
            holder = l;
            return utils::helper::defer([&] {
                holder = old;
            });
        }
    };

    template <class T>
    void extract_call(CallHolder& h, T& c) {
        if (ast::Call* call = ast::as<ast::Call>(c)) {
            ast::objlist args;
            auto l = h.enter_holder(&args);
            extract_call(h, call->callee);
            extract_call(h, call->arguments);
            std::shared_ptr<ast::TmpVar> v = std::make_shared<ast::TmpVar>(std::static_pointer_cast<ast::Call>(c));
            v->inner = std::move(args);
            c = v;
            return;
        }
        ast::traverse(c, [&](auto&& f) {
            extract_call(h, f);
        });
    }
}  // namespace vm
