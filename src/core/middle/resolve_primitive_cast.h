/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>
#include <core/common/expected.h>

namespace brgen::middle {

    // call before typing
    result<void> resolve_primitive_cast(auto& node) {
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
                    if (!i_desc &&
                        ident->ident != "void" &&
                        ident->ident != "bool") {
                        return;
                    }
                    if (p->arguments.size() != 1) {
                        error(p->loc, "invalid argument count for primitive cast").report();
                        return;
                    }
                    ident->usage = ast::IdentUsage::reference_type;
                    std::shared_ptr<ast::Type> type;
                    if (i_desc) {
                        type = std::make_shared<ast::IntType>(ident->loc, i_desc->bit_size, i_desc->endian, i_desc->is_signed);
                    }
                    else if (ident->ident == "void") {
                        type = std::make_shared<ast::VoidType>(ident->loc);
                    }
                    else if (ident->ident == "bool") {
                        type = std::make_shared<ast::BoolType>(ident->loc);
                    }
                    else {
                        assert(false);
                    }
                    auto arg = p->arguments[0];
                    node = std::make_shared<ast::Cast>(ast::cast_to<ast::Call>(std::move(node)), std::move(type), std::move(arg));
                }
            }
        };
        try {
            f(f, node);
        } catch (const LocationError& e) {
            return brgen::unexpect(e);
        }
        return {};
    }

}  // namespace brgen::middle
