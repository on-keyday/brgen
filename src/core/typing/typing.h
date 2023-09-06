/*license*/
#pragma once
#include "../lexer/token.h"
#include "../common/file.h"
#include "../ast/ast.h"
#include "../ast/translated.h"
#include "../ast/traverse.h"

namespace brgen::typing {

    struct Typing {
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

       private:
        auto void_type(lexer::Loc loc) {
            return std::make_shared<ast::VoidType>(loc);
        }

        [[noreturn]] void report_not_equal_type(const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            error(lty->loc, "type not equal here").error(rty->loc, "and here").report();
        }

        [[noreturn]] void unsupported(auto&& expr) {
            if (auto ident = ast::as<ast::Ident>(expr)) {
                error(ident->loc, "identifier ", ident->ident, " is not defined").report();
            }
            error(expr->loc, "unsupported operation").report();
        }

        void check_bool(ast::Expr* expr) {
            if (!expr->expr_type) {
                unsupported(expr);
            }
            if (expr->expr_type->type != ast::NodeType::bool_type) {
                error(expr->loc, "expect bool but not").report();
            }
        }

        std::shared_ptr<ast::Type> assigning_type(const std::shared_ptr<ast::Type>& base) {
            if (auto ty = ast::as<ast::IntLiteralType>(base)) {
                auto aligned = ty->get_aligned_bit();
                return std::make_shared<ast::IntegerType>(ty->loc, "", aligned);
            }
            return base;
        }

        void int_type_fitting(std::shared_ptr<ast::Type>& left, std::shared_ptr<ast::Type>& right) {
            auto fitting = [&](auto& a, auto& b) {
                auto ity = ast::as<ast::IntegerType>(a);
                auto lty = ast::as<ast::IntLiteralType>(b);
                auto bit_size = lty->get_bit_size();
                if (ity->bit_size < *bit_size) {
                    error(lty->loc, "bit size ", nums(*bit_size), " is too large")
                        .error(ity->loc, "for this")
                        .report();
                }
                b = a;  // fitting
            };
            if (left->type == ast::NodeType::int_type &&
                right->type == ast::NodeType::int_literal_type) {
                fitting(left, right);
            }
            else if (left->type == ast::NodeType::int_literal_type &&
                     right->type == ast::NodeType::int_type) {
                fitting(right, left);
            }
            else if (left->type == ast::NodeType::int_literal_type &&
                     right->type == ast::NodeType::int_literal_type) {
                left = assigning_type(left);
                right = assigning_type(right);
                auto li = ast::as<ast::IntegerType>(left);
                auto ri = ast::as<ast::IntegerType>(right);
                if (li->bit_size > ri->bit_size) {
                    right = left;
                }
                else {
                    left = right;
                }
            }
        }

        void typing_assign(ast::Binary* b) {
            auto left = ast::as<ast::Ident>(b->left);
            auto right = b->right;
            if (!right->expr_type) {
                unsupported(right);
            }
            b->expr_type = void_type(b->loc);
            auto base = left->base.lock();
            auto report_assign_error = [&] {
                error(b->loc, "cannot assign to ", left->ident).error(base->loc, "ident ", left->ident, " is defined here").report();
            };
            auto new_type = assigning_type(right->expr_type);
            if (b->op == ast::BinaryOp::assign) {
                if (left->usage == ast::IdentUsage::unknown) {
                    left->usage = ast::IdentUsage::define_alias;
                    left->expr_type = std::move(new_type);
                }
                else {
                    if (base->usage == ast::IdentUsage::define_alias) {
                        if (!equal_type(base->expr_type, new_type)) {
                            left->usage = ast::IdentUsage::define_alias;
                            left->expr_type = right->expr_type;
                            left->base.reset();
                        }
                    }
                    else if (base->usage == ast::IdentUsage::define_typed) {
                        if (!equal_type(base->expr_type, new_type)) {
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
                    left->expr_type = std::move(new_type);
                }
                else {
                    report_assign_error();
                }
            }
            else if (b->op == ast::BinaryOp::const_assign) {
                if (left->usage == ast::IdentUsage::unknown) {
                    left->usage = ast::IdentUsage::define_const;
                    left->expr_type = std::move(new_type);
                }
                else {
                    report_assign_error();
                }
            }
        }

        std::shared_ptr<ast::Type> extract_expr_type(const std::shared_ptr<ast::IndentScope>& block) {
            auto last_element = block->elements.back();
            if (auto then_expr = ast::as<ast::Expr>(last_element)) {
                return then_expr->expr_type;
            }
            return nullptr;
        }

        std::shared_ptr<ast::Type> extract_else_type(const std::shared_ptr<ast::Node>& els) {
            if (!els) {
                return nullptr;
            }

            if (auto expr = ast::as<ast::Expr>(els)) {
                return expr->expr_type;
            }

            if (auto block = ast::as<ast::IndentScope>(els)) {
                auto expr = ast::as<ast::Expr>(block->elements.back());
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
            if (if_->els) {
                typing_object(if_->els);
            }

            auto then_ = extract_expr_type(if_->block);
            auto els_ = extract_else_type(if_->els);

            if (!then_ || !els_ ||
                (int_type_fitting(then_, els_), !equal_type(then_, els_))) {
                if_->expr_type = void_type(if_->loc);
                return;
            }

            if_->expr_type = then_;

            auto& then_ref = if_->block->elements.back();

            then_ref = std::make_shared<ast::ImplicitReturn>(std::static_pointer_cast<ast::Expr>(then_ref));
            if (auto block = ast::as<ast::IndentScope>(if_->els)) {
                auto& else_ref = block->elements.back();

                else_ref = std::make_shared<ast::ImplicitReturn>(std::static_pointer_cast<ast::Expr>(else_ref));
            }
        }

        void typing_binary(ast::Binary* b) {
            auto op = b->op;
            auto& lty = b->left->expr_type;
            auto& rty = b->right->expr_type;
            auto report_binary_error = [&] {
                error(b->loc, "binary op ", *ast::bin_op_str(b->op), " is not valid")
                    .report();
            };
            if (!lty) {
                unsupported(b->left);
            }
            if (!rty) {
                unsupported(b->right);
            }
            int_type_fitting(lty, rty);
            switch (op) {
                case ast::BinaryOp::left_shift:
                case ast::BinaryOp::right_shift: {
                    if (lty->type == ast::NodeType::int_type &&
                        rty->type == ast::NodeType::int_type) {
                        b->expr_type = std::move(lty);
                        return;
                    }
                    report_binary_error();
                }
                case ast::BinaryOp::bit_and: {
                    if (lty->type == ast::NodeType::int_type &&
                        rty->type == ast::NodeType::int_type) {
                        if (!equal_type(rty, lty)) {
                            report_not_equal_type(rty, lty);
                        }
                        b->expr_type = std::move(lty);
                        return;
                    }
                    report_binary_error();
                }
                case ast::BinaryOp::equal:
                case ast::BinaryOp::not_equal: {
                    if (!equal_type(lty, rty)) {
                        report_not_equal_type(lty, rty);
                    }
                    b->expr_type = std::make_shared<ast::BoolType>(b->loc);
                    return;
                }
                default: {
                    report_binary_error();
                }
            }
        }

        std::optional<std::shared_ptr<ast::Ident>> find_matching_ident(ast::Ident* ident) {
            return ident->frame->lookup<std::shared_ptr<ast::Ident>>([&](ast::Definitions& defs) -> std::optional<std::shared_ptr<ast::Ident>> {
                auto found = defs.idents.find(ident->ident);
                if (found != defs.idents.end()) {
                    for (auto it = found->second.rbegin(); it != found->second.rend(); it++) {
                        auto& rev = *it;
                        auto usage = rev->usage;
                        if (usage != ast::IdentUsage::unknown) {
                            return rev;
                        }
                    }
                }
                return std::nullopt;
            });
        }

        void typing_binary_expr(ast::Binary* bin) {
            auto op = bin->op;
            typing_expr(bin->left);
            typing_expr(bin->right);
            switch (op) {
                case ast::BinaryOp::assign:
                case ast::BinaryOp::typed_assign:
                case ast::BinaryOp::const_assign:
                    typing_assign(bin);
                    break;
                default:
                    typing_binary(bin);
            }
        }

        std::optional<std::shared_ptr<ast::Fmt>> find_matching_fmt(ast::IdentType* ident) {
            return ident->frame->lookup<std::shared_ptr<ast::Fmt>>([&](ast::Definitions& defs) -> std::optional<std::shared_ptr<ast::Fmt>> {
                auto found = defs.fmts.find(ident->ident);
                if (found != defs.fmts.end()) {
                    for (auto it = found->second.rbegin(); it != found->second.rend(); it++) {
                        return found->second.back();
                    }
                }
                return std::nullopt;
            });
        }

        void typing_expr(const std::shared_ptr<ast::Expr>& expr) {
            if (auto lit = ast::as<ast::IntLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::IntLiteralType>(std::static_pointer_cast<ast::IntLiteral>(expr));
            }
            else if (auto lit = ast::as<ast::BoolLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::BoolType>(lit->loc);
            }
            else if (auto ident = ast::as<ast::Ident>(expr)) {
                if (ident->usage != ast::IdentUsage::unknown) {
                    if (auto ident_ty = ast::as<ast::IdentType>(ident->expr_type)) {
                    }
                }
                auto found = find_matching_ident(ident);
                if (found) {
                    ident->expr_type = (*found)->expr_type;
                    ident->base = *found;
                    ident->usage = ast::IdentUsage::reference;
                }
            }
            else if (auto bin = ast::as<ast::Binary>(expr)) {
                typing_binary_expr(bin);
            }
            else if (auto if_ = ast::as<ast::If>(expr)) {
                typing_if(if_);
            }
            else if (auto cond = ast::as<ast::Cond>(expr)) {
                typing_expr(cond->cond);
                typing_expr(cond->then);
                typing_expr(cond->els);
                auto lty = cond->then->expr_type;
                auto rty = cond->els->expr_type;
                int_type_fitting(lty, rty);
                if (!equal_type(lty, rty)) {
                    report_not_equal_type(lty, rty);
                }
                cond->expr_type = lty;
            }
            else if (auto paren = ast::as<ast::Paren>(expr)) {
                typing_expr(paren->expr);
                paren->expr_type = paren->expr->expr_type;
            }
            else if (auto unary = ast::as<ast::Unary>(expr)) {
                typing_expr(unary->target);
                if (!unary->target->expr_type) {
                    unsupported(unary->target);
                }
                switch (unary->op) {
                    case ast::UnaryOp::minus_sign: {
                        unary->expr_type = unary->target->expr_type;
                        break;
                    }
                    default: {
                        unsupported(expr);
                    }
                }
            }
            else if (auto call = ast::as<ast::Call>(expr)) {
                typing_expr(call->callee);
            }
            else if (auto selector = ast::as<ast::MemberAccess>(expr)) {
                typing_expr(selector->target);
            }
            else {
                unsupported(expr);
            }
        }

        void typing_object(const std::shared_ptr<ast::Node>& ty) {
            // Define a lambda function for recursive traversal and typing
            auto recursive_typing = [&](auto&& f, const std::shared_ptr<ast::Node>& ty) -> void {
                if (auto expr = ast::as<ast::Expr>(ty)) {
                    // If the object is an expression, perform expression typing
                    typing_expr(std::static_pointer_cast<ast::Expr>(ty));
                    return;
                }
                // Traverse the object's subcomponents and apply the recursive function
                ast::traverse(ty, [&](const std::shared_ptr<ast::Node>& sub_ty) {
                    f(f, sub_ty);
                });
            };
            recursive_typing(recursive_typing, ty);
        }

       public:
        inline result<void> typing(const std::shared_ptr<ast::Node>& ty) {
            try {
                typing_object(ty);
            } catch (LocationError& e) {
                return unexpect(e);
            }
            return {};
        }
    };

}  // namespace brgen::typing
