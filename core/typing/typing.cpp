/*license*/
#include "ast/ast.h"
#include "ast/util.h"
#include "ast/stream.h"

namespace typing {

    void typing_object(const std::shared_ptr<ast::Object>& ty);

    bool equal_type(const std::shared_ptr<ast::Type>& left, const std::shared_ptr<ast::Type>& right) {
        if (left->type != right->type) {
            return false;
        }
        if (auto lty = ast::as<ast::IntegerType>(left)) {
            auto rty = ast::as<ast::IntegerType>(right);
            return lty->bit_size == rty->bit_size;
        }
        if (auto lty = ast::as<ast::IdentType>(left)) {
            auto rty = ast::as<ast::IdentType>(right);
            return lty->ident == rty->ident;
        }
        return false;
    }

    void typeing_assign(ast::Binary* b) {
        auto left = ast::as<ast::Ident>(b->left);
        auto right = b->right;
        if (b->op == ast::BinaryOp::assign) {
            if (left->usage == ast::IdentUsage::unknown) {
                left->usage = ast::IdentUsage::define_alias;
                left->expr_type = right->expr_type;
            }
        }
        if (b->op == ast::BinaryOp::typed_assign) {
            if (left->usage == ast::IdentUsage::unknown) {
                left->usage = ast::IdentUsage::define_typed;
                left->expr_type = right->expr_type;
            }
            else {
            }
        }
    }

    void typing_expr(ast::Expr* expr) {
        if (auto lit = ast::as<ast::IntLiteral>(expr)) {
            lit->expr_type = std::make_shared<ast::IntegerType>(lit->loc, "u32", 32);
        }
        else if (auto ident = ast::as<ast::Ident>(expr)) {
            auto found = ident->frame->lookup<std::shared_ptr<ast::Ident>>([&](ast::Definitions& defs) -> std::optional<std::shared_ptr<ast::Ident>> {
                auto found = defs.idents.find(ident->ident);
                if (found != defs.idents.end()) {
                    if (found->second.front()->usage != ast::IdentUsage::unknown) {
                        return found->second.front();
                    }
                }
                return std::nullopt;
            });
            if (found) {
                ident->expr_type = (*found)->expr_type;
                ident->base = *found;
                ident->usage = ast::IdentUsage::reference;
            }
        }
        else if (auto bin = ast::as<ast::Binary>(expr)) {
            typing_expr(bin->left.get());
            typing_expr(bin->right.get());
            switch (bin->op) {
                case ast::BinaryOp::assign:
                case ast::BinaryOp::typed_assign:
                case ast::BinaryOp::const_assign: {
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
