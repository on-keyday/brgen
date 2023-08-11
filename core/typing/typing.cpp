/*license*/
#include "../ast/ast.h"
#include "../ast/util.h"
#include "typing.h"
#include <ranges>

namespace brgen::typing {

    void typing_object(const std::shared_ptr<ast::Object>& ty);

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

    static void typing_assign(ast::Binary* b) {
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

    [[noreturn]] static void report_not_eqaul_type(const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
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
            auto lty = ast::as<ast::IntLiteralType>(left);
            auto rty = ast::as<ast::IntLiteralType>(right);
            auto lsize = lty->get_bit_size();
            auto rsize = rty->get_bit_size();
            auto larger = *lsize < *rsize ? *rsize : *lsize;
        }
    }

    static std::shared_ptr<ast::Type> extract_expr_type(const std::shared_ptr<ast::IndentScope>& block) {
        auto last_element = block->elements.back();
        if (auto then_expr = ast::as_Expr(last_element)) {
            return then_expr->expr_type;
        }
        return nullptr;
    }

    static std::shared_ptr<ast::Type> extract_else_type(const std::shared_ptr<ast::Object>& els) {
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

        if (!then_ || !els_ || !equal_type(then_, els_)) {
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
                        report_not_eqaul_type(rty, lty);
                    }
                    b->expr_type = std::move(lty);
                    return;
                }
                report_binary_error();
            }
            case ast::BinaryOp::equal: {
                if (!equal_type(lty, rty)) {
                    report_not_eqaul_type(lty, rty);
                }
                b->expr_type = std::make_shared<ast::BoolType>(b->loc);
                return;
            }
            default: {
                report_binary_error();
            }
        }
    }

    static void typing_expr(const std::shared_ptr<ast::Expr>& expr) {
        if (auto lit = ast::as<ast::IntLiteral>(expr)) {
            lit->expr_type = std::make_shared<ast::IntLiteralType>(std::static_pointer_cast<ast::IntLiteral>(expr));
        }
        else if (auto ident = ast::as<ast::Ident>(expr)) {
            if (ident->usage != ast::IdentUsage::unknown) {
                auto usage = ident->usage;
                return;  // nothing to do
            }
            auto found = ident->frame->lookup<std::shared_ptr<ast::Ident>>([&](ast::Definitions& defs) -> std::optional<std::shared_ptr<ast::Ident>> {
                auto found = defs.idents.find(ident->ident);
                if (found != defs.idents.end()) {
                    for (auto& rev : std::ranges::reverse_view(found->second)) {
                        auto usage = rev->usage;
                        if (usage != ast::IdentUsage::unknown) {
                            return rev;
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
            auto op = bin->op;
            typing_expr(bin->left);
            typing_expr(bin->right);
            switch (op) {
                case ast::BinaryOp::assign:
                case ast::BinaryOp::typed_assign:
                case ast::BinaryOp::const_assign: {
                    typing_assign(bin);
                    break;
                }
                default: {
                    typing_binary(bin);
                }
            }
        }
        else if (auto if_ = ast::as<ast::If>(expr)) {
            typing_if(if_);
        }
        else {
            throw UnsupportedError{expr->loc};
        }
    }

    void typing_object(const std::shared_ptr<ast::Object>& ty) {
        // Define a lambda function for recursive traversal and typing
        auto recursive_typing = [&](auto&& f, const std::shared_ptr<ast::Object>& ty) -> void {
            if (auto expr = ast::as_Expr(ty)) {
                // If the object is an expression, perform expression typing
                typing_expr(std::static_pointer_cast<ast::Expr>(ty));
                return;
            }
            // Traverse the object's subcomponents and apply the recursive function
            ast::traverse(ty, [&](const std::shared_ptr<ast::Object>& sub_ty) {
                f(f, sub_ty);
            });
        };
        recursive_typing(recursive_typing, ty);
    }
}  // namespace brgen::typing
