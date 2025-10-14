/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>
#include "core/ast/node/type.h"

namespace brgen::middle {

    result<void> resolve_available(auto& node) {
        auto f = [](auto&& f, auto& node) -> void {
            // traverse child first
            ast::traverse(node, [&](auto& n) {
                f(f, n);
            });
            using T = std::decay_t<decltype(node)>;
            using P = typename futils::helper::template_of_t<T>::template param_at<0>;
            if constexpr (std::is_base_of_v<P, ast::Call> && !std::is_same_v<P, ast::Call>) {
                if (ast::Call* p = ast::as<ast::Call>(node)) {
                    auto ident = ast::as<ast::Ident>(p->callee);
                    if (!ident) {
                        return;
                    }
                    if (ident->ident != "available" && ident->ident != "sizeof") {
                        return;
                    }
                    if (p->arguments.size() < 1) {
                        error(p->loc, "invalid ", ident->ident, "; must have at least one argument").report();
                    }
                    if (!ast::as<ast::MemberAccess>(p->arguments[0]) && !ast::as<ast::Ident>(p->arguments[0])) {
                        error(p->arguments[0]->loc, "invalid target of ", ident->ident, "; must be an ident or member access").report();
                    }
                    ident->usage = ast::IdentUsage::reference_builtin_fn;
                    auto t = p->arguments[0];
                    if (ident->ident == "available") {
                        auto a = std::make_shared<ast::Available>(std::move(t), ast::cast_to<ast::Call>(std::move(node)));
                        a->expr_type = std::make_shared<ast::BoolType>(ident->loc);
                        node = std::move(a);
                    }
                    else {
                        auto a = std::make_shared<ast::SizeOf>(std::move(t), ast::cast_to<ast::Call>(std::move(node)));
                        a->expr_type = std::make_shared<ast::IntType>(ident->loc, 64, ast::Endian::unspec, false);
                        node = std::move(a);
                    }
                }
            }
        };
        try {
            f(f, node);
        } catch (LocationError& e) {
            return unexpect(e);
        }
        return {};
    }
}  // namespace brgen::middle
