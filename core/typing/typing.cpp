/*license*/
#include "ast/ast.h"
#include "ast/util.h"
#include "ast/stream.h"

namespace typing {

    void typing_object(const std::shared_ptr<ast::Object>& ty);

    bool equal_type() {}

    void typing_expr(ast::Expr* expr) {
        if (auto lit = ast::as<ast::IntLiteral>(expr)) {
            lit->expr_type = std::make_shared<ast::IntegerType>(lit->loc, "u32", 32);
        }
        else if (auto ident = ast::as<ast::Ident>(expr)) {
            auto ty = ident->frame->lookup_first<std::shared_ptr<ast::Type>>([&](ast::Definitions& defs) -> std::optional<std::shared_ptr<ast::Type>> {
                auto found = defs.idents.find(ident->ident);
                if (found != defs.idents.end()) {
                    return found->second.front()->expr_type;
                }
                return std::nullopt;
            });
            if (ty) {
                ident->expr_type = std::move(*ty);
            }
        }
        else if (auto bin = ast::as<ast::Binary>(expr)) {
            typing_expr(bin->left.get());
            typing_expr(bin->right.get());
            switch (bin->op) {
                case ast::BinaryOp::assign: {
                    if (bin->left->expr_type) {
                    }
                }
            }
        }
        else if (auto if_ = ast::as<ast::If>(expr)) {
        }
    }

    void typing_object(const std::shared_ptr<ast::Object>& ty) {
        auto f = [&](auto&& f, const std::shared_ptr<ast::Object>& ty) -> void {
            if (auto expr = ast::as_Expr(ty)) {
                typing_expr(expr);
            }
            ast::traverse(ty, [&](const std::shared_ptr<ast::Object>& ty) {
                f(f, ty);
            });
        };
        ast::traverse(ty, [&](const std::shared_ptr<ast::Object>& ty) {
            f(f, ty);
        });
    }
}  // namespace typing