/*license*/
#include "ast/ast.h"
#include "ast/util.h"
#include "ast/stream.h"
#include "typing.h"

namespace brgen::typing {

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

    auto void_type(lexer::Loc loc) {
        return std::make_shared<ast::VoidType>(loc);
    }

    void typing_assign(ast::Binary* b) {
        auto left = ast::as<ast::Ident>(b->left);
        auto right = b->right;
        b->expr_type = void_type(b->loc);
        auto base = left->base.lock();
        auto report_assign_error = [&] {
            throw AssignError{
                left->ident,
                b->loc,
                base->loc,
            };
        };
        if (b->op == ast::BinaryOp::assign) {
            if (left->usage == ast::IdentUsage::unknown) {
                left->usage = ast::IdentUsage::define_alias;
                left->expr_type = right->expr_type;
            }
            else {
                if (base->usage == ast::IdentUsage::define_alias) {
                    if (!equal_type(base->expr_type, right->expr_type)) {
                        left->usage = ast::IdentUsage::define_alias;
                        left->expr_type = right->expr_type;
                        left->base.reset();
                    }
                }
                else if (base->usage == ast::IdentUsage::define_typed) {
                    if (!equal_type(base->expr_type, right->expr_type)) {
                        report_assign_error();
                    }
                }
                else if (base->usage == ast::IdentUsage::define_const) {
                    report_assign_error();
                }
            }
        }
        else if (b->op == ast::BinaryOp::typed_assign) {
            if (left->usage == ast::IdentUsage::unknown) {
                left->usage = ast::IdentUsage::define_typed;
                left->expr_type = right->expr_type;
            }
            else {
                report_assign_error();
            }
        }
        else if (b->op == ast::BinaryOp::const_assign) {
            if (left->usage == ast::IdentUsage::unknown) {
                left->usage = ast::IdentUsage::define_const;
                left->expr_type = right->expr_type;
            }
            else {
                report_assign_error();
            }
        }
    }

    void check_bool(ast::Expr* expr) {
        if (expr->type != ast::ObjectType::bool_type) {
            throw NotBoolError{expr->loc};
        }
    }

    std::shared_ptr<ast::Type> extract_expr_type(const std::shared_ptr<ast::IndentScope>& block) {
        auto last_element = block->elements.back();
        if (auto then_expr = ast::as_Expr(last_element)) {
            return then_expr->expr_type;
        }
        return nullptr;
    }

    std::shared_ptr<ast::Type> extract_else_type(const std::shared_ptr<ast::Object>& els) {
        if (!els) {
            return nullptr;
        }

        if (auto expr = ast::as_Expr(els)) {
            return expr->expr_type;
        }

        if (auto block = ast::as<ast::IndentScope>(els)) {
            auto expr = ast::as_Expr(block->elements.back());
            if (expr) {
                return expr->expr_type;
            }
        }

        return nullptr;
    }

    void typing_if(ast::If* if_) {
        typing_object(if_->cond);
        check_bool(if_->cond.get());
        typing_object(if_->block);

        auto then_ = extract_expr_type(if_->block);
        auto els_ = extract_else_type(if_->els);

        if (!then_ || !els_ || !equal_type(then_, els_)) {
            if_->expr_type = void_type(if_->loc);
            return;
        }

        if_->expr_type = then_;
    }

    void typing_binary(ast::Binary* b) {
        switch (b->op) {
            case ast::BinaryOp::left_shift: {
                if (b->left->type == ast::ObjectType::int_type) {
                }
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
                    for (auto& rev : std::ranges::reverse_view(found->second)) {
                        if (rev->usage != ast::IdentUsage::unknown) {
                            return found->second.front();
                        }
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
                    typing_assign(bin);
                    break;
                }
                case ast::BinaryOp::left_shift:
                case ast::BinaryOp::right_shift:
                case ast::BinaryOp::bit_and: {
                    typing_binary(bin);
                    break;
                }
            }
        }
    }

    void typing_object(const std::shared_ptr<ast::Object>& ty) {
        // Define a lambda function for recursive traversal and typing
        auto recursive_typing = [&](auto&& f, const std::shared_ptr<ast::Object>& ty) -> void {
            if (auto expr = ast::as_Expr(ty)) {
                // If the object is an expression, perform expression typing
                typing_expr(expr);
            }
            // Traverse the object's subcomponents and apply the recursive function
            ast::traverse(ty, [&](const std::shared_ptr<ast::Object>& sub_ty) {
                f(f, sub_ty);
            });
        };

        // Start the recursive traversal by invoking the lambda
        ast::traverse(ty, [&](const std::shared_ptr<ast::Object>& sub_ty) {
            recursive_typing(recursive_typing, sub_ty);
        });
    }
}  // namespace brgen::typing
