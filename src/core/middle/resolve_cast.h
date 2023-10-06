/*license*/
#pragma once
#include <core/ast/traverse.h>

namespace brgen::middle {

    void resolve_cast(const std::shared_ptr<ast::Node>& node) {
        auto f = [](auto&& f, auto& node) -> void {
            // traverse child first
            ast::traverse(node, [&](auto& n) {
                f(f, n);
            });
            using T = std::decay_t<decltype(node)>;
            using P = typename utils::helper::template_of_t<T>::template param_at<0>;
            if constexpr (std::is_base_of_v<P, ast::Call>) {
                if (ast::Call* p = ast::as<ast::Call>(node)) {
                    auto ident = ast::as<ast::Ident>(p->callee);
                    if (!ident) {
                        return;
                    }
                    auto i_desc = ast::is_int_type(ident->ident);
                    if (!i_desc) {
                        return;
                    }
                }
            }
        };
        traverse(node, [&](auto& n) {
            f(f, n);
        });
    }
}  // namespace brgen::middle
