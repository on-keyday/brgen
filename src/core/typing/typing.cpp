/*license*/
#include "../ast/ast.h"
#include "../ast/traverse.h"
#include "typing.h"
#include <ranges>

namespace brgen::typing {

    void typing_object(const std::shared_ptr<ast::Node>& ty);

    static bool equal_type(const std::shared_ptr<ast::Type>& left, const std::shared_ptr<ast::Type>& right) {
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

    static auto void_type(lexer::Loc loc) {
        return std::make_shared<ast::VoidType>(loc);
    }

    [[noreturn]] static void report_not_equal_type(const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
        throw NotEqualTypeError{lty->loc, rty->loc};
    }

    [[noreturn]] static void unsupported_expr(auto&& expr) {
        if (auto ident = ast::as<ast::Ident>(expr)) {
            throw NotDefinedError{
                ident->ident,
                ident->loc,
            };
        }
        throw UnsupportedError{expr->loc};
    }

    static void check_bool(ast::Expr* expr) {
        if (!expr->expr_type) {
            unsupported_expr(expr);
        }
        if (expr->expr_type->type != ast::ObjectType::bool_type) {
            throw NotBoolError{expr->loc};
        }
    }

    static std::shared_ptr<ast::Type> assigning_type(const std::shared_ptr<ast::Type>& base) {
        if (auto ty = ast::as<ast::IntLiteralType>(base)) {
            auto aligned = ty->aligned_bit();
            return std::make_shared<ast::IntegerType>(ty->loc, "", aligned);
        }
        return base;
    }

    static void int_type_fitting(std::shared_ptr<ast::Type>& left, std::shared_ptr<ast::Type>& right) {
        auto fitting = [&](auto& a, auto& b) {
            auto ity = ast::as<ast::IntegerType>(a);
            auto lty = ast::as<ast::IntLiteralType>(b);
            auto bit_size = lty->get_bit_size();
            if (ity->bit_size < *bit_size) {
                throw TooLargeError{lty->loc};
            }
            b = a;  // fitting
        };
        if (left->type == ast::ObjectType::int_type &&
            right->type == ast::ObjectType::int_literal_type) {
            fitting(left, right);
        }
        else if (left->type == ast::ObjectType::int_literal_type &&
                 right->type == ast::ObjectType::int_type) {
            fitting(right, left);
        }
        else if (left->type == ast::ObjectType::int_literal_type &&
                 right->type == ast::ObjectType::int_literal_type) {
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

    static void typing_assign(ast::Binary* b) {
        auto left = ast::as<ast::Ident>(b->left);
        auto right = b->right;
        if (!right->expr_type) {
            unsupported_expr(right);
        }
        b->expr_type = void_type(b->loc);
        auto base = left->base.lock();
        auto report_assign_error = [&] {
            throw AssignError{
                left->ident,
                b->loc,
                base->loc,
            };
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

    static std::shared_ptr<ast::Type> extract_expr_type(const std::shared_ptr<ast::IndentScope>& block) {
        auto last_element = block->elements.back();
        if (auto then_expr = ast::as_Expr(last_element)) {
            return then_expr->expr_type;
        }
        return nullptr;
    }

    static std::shared_ptr<ast::Type> extract_else_type(const std::shared_ptr<ast::Node>& els) {
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

    static void typing_if(ast::If* if_) {
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
    }

    static void typing_binary(ast::Binary* b) {
        auto op = b->op;
        auto& lty = b->left->expr_type;
        auto& rty = b->right->expr_type;
        auto report_binary_error = [&] {
            throw BinaryOpTypeError{b->op, b->loc};
        };
        if (!lty) {
            unsupported_expr(b->left);
        }
        if (!rty) {
            unsupported_expr(b->right);
        }
        int_type_fitting(lty, rty);
        switch (op) {
            case ast::BinaryOp::left_shift:
            case ast::BinaryOp::right_shift: {
                if (lty->type == ast::ObjectType::int_type &&
                    rty->type == ast::ObjectType::int_type) {
                    b->expr_type = std::move(lty);
                    return;
                }
                report_binary_error();
            }
            case ast::BinaryOp::bit_and: {
                if (lty->type == ast::ObjectType::int_type &&
                    rty->type == ast::ObjectType::int_type) {
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

    static std::optional<std::shared_ptr<ast::Ident>> find_matching_ident(ast::Ident* ident) {
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

    static void typing_expr(const std::shared_ptr<ast::Expr>& expr);

    static void typing_binary_expr(ast::Binary* bin) {
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

    static void typing_expr(const std::shared_ptr<ast::Expr>& expr) {
        if (auto lit = ast::as<ast::IntLiteral>(expr)) {
            lit->expr_type = std::make_shared<ast::IntLiteralType>(std::static_pointer_cast<ast::IntLiteral>(expr));
        }
        else if (auto lit = ast::as<ast::BoolLiteral>(expr)) {
            lit->expr_type = std::make_shared<ast::BoolType>(lit->loc);
        }
        else if (auto ident = ast::as<ast::Ident>(expr)) {
            if (ident->usage != ast::IdentUsage::unknown) {
                return;  // nothing to do
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
                unsupported_expr(unary->target);
            }
            switch (unary->op) {
                case ast::UnaryOp::minus_sign: {
                    unary->expr_type = unary->target->expr_type;
                    break;
                }
                default: {
                    unsupported_expr(expr);
                }
            }
        }
        else if (auto call = ast::as<ast::Call>(expr)) {
            typing_expr(call->callee);
        }
        else if (auto selector = ast::as<ast::MemberAccess>(expr)) {
            typing_expr(selector->target);
            selector->name;
        }
        else {
            throw UnsupportedError{expr->loc};
        }
    }

    void typing_object(const std::shared_ptr<ast::Node>& ty) {
        // Define a lambda function for recursive traversal and typing
        auto recursive_typing = [&](auto&& f, const std::shared_ptr<ast::Node>& ty) -> void {
            if (auto expr = ast::as_Expr(ty)) {
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
}  // namespace brgen::typing
