/*license*/
#pragma once
#include <core/ast/traverse.h>

namespace brgen::middle {

    void resolve_available(auto& node) {
        auto f = [](auto&& f, auto& node) -> void {
            // traverse child first
            ast::traverse(node, [&](auto& n) {
                f(f, n);
            });
            using T = std::decay_t<decltype(node)>;
            using P = typename utils::helper::template_of_t<T>::template param_at<0>;
            if constexpr (std::is_base_of_v<P, ast::Call> && !std::is_same_v<P, ast::Call>) {
                if (ast::Call* p = ast::as<ast::Call>(node)) {
                    auto ident = ast::as<ast::Ident>(p->callee);
                    if (!ident) {
                        return;
                    }
                    if (ident->ident != "available") {
                        return;
                    }
                    if (p->arguments.size() != 1) {
                        return;
                    }
                    auto target = ast::as<ast::Ident>(p->arguments[0]);
                    if (!target) {
                        return;
                    }
                    ident->usage = ast::IdentUsage::reference_builtin_fn;
                    auto a = std::make_shared<ast::Available>(ast::cast_to<ast::Ident>(p->arguments[0]), ast::cast_to<ast::Call>(std::move(node)));
                    a->expr_type = std::make_shared<ast::BoolType>(ident->loc);
                    node = std::move(a);
                }
            }
        };
        f(f, node);
    }
}  // namespace brgen::middle
