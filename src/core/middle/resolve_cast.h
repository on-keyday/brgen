/*license*/
#pragma once
#include <core/ast/traverse.h>

namespace brgen::middle {

    void resolve_cast(const std::shared_ptr<ast::Node>& node) {
        auto f = [](auto&& f, auto& node) -> void {
            ast::traverse(node, [&](auto& n) {
                f(f, n);
            });
            using T = std::decay_t<decltype(node)>;
            using P = typename utils::helper::template_of_t<T>::template param_at<0>;
            if constexpr (std::is_base_of<P, ast::Call>) {
                if (ast::Call* p = ast::as<ast::Call>(node)) {
                    if (p->callee->expr_type != ast::NodeType::ident) {
                    }
                }
            }
        };
        traverse(node, [&](auto& n) {
            f(f, n);
        });
    }
}  // namespace brgen::middle
