/*license*/
#pragma once
#include <core/ast/traverse.h>

namespace brgen::middle {

    // call before typing
    void resolve_int_cast(auto& node) {
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
                    auto i_desc = ast::is_int_type(ident->ident);
                    if (!i_desc) {
                        return;
                    }
                    if (p->arguments.size() != 1) {
                        return;
                    }
                    ident->usage = ast::IdentUsage::reference_type;
                    auto int_type = std::make_shared<ast::IntType>(ident->loc, i_desc->bit_size, i_desc->endian, i_desc->is_signed);
                    auto arg = p->arguments[0];
                    node = std::make_shared<ast::Cast>(ast::cast_to<ast::Call>(std::move(node)), std::move(int_type), std::move(arg));
                }
            }
        };
        f(f, node);
    }

}  // namespace brgen::middle
