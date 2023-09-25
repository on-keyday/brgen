/*license*/
#pragma once
#include "../lexer/token.h"
#include "../common/file.h"
#include <core/ast/traverse.h>
#include <helper/defer.h>

namespace brgen::middle {

    enum class TypeRelation {
        equal,
        each_convertible,
        left_convertible,
        right_convertible,
        not_equal,
    };

    struct Typing {
        LocationError warnings;

        std::shared_ptr<ast::Scope> current_global;
        bool equal_type(const std::shared_ptr<ast::Type>& left, const std::shared_ptr<ast::Type>& right) {
            if (left->node_type != right->node_type) {
                return false;
            }
            if (auto lty = ast::as<ast::IntType>(left)) {
                auto rty = ast::as<ast::IntType>(right);
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

        void warn_not_typed(auto&& expr) {
            warnings.warning(expr->loc, "expression is not typed yet");
        }

        void check_bool(ast::Expr* expr) {
            if (!expr->expr_type) {
                unsupported(expr);
            }
            if (expr->expr_type->node_type != ast::NodeType::bool_type) {
                error(expr->loc, "expect bool but not").report();
            }
        }

        std::shared_ptr<ast::Type> assigning_type(const std::shared_ptr<ast::Type>& base) {
            if (auto ty = ast::as<ast::IntLiteralType>(base)) {
                auto aligned = ty->get_aligned_bit();
                return std::make_shared<ast::IntType>(ty->loc, "", aligned);
            }
            return base;
        }

        void int_type_fitting(std::shared_ptr<ast::Type>& left, std::shared_ptr<ast::Type>& right) {
            auto fitting = [&](auto& a, auto& b) {
                auto ity = ast::as<ast::IntType>(a);
                auto lty = ast::as<ast::IntLiteralType>(b);
                auto bit_size = lty->get_bit_size();
                if (ity->bit_size < *bit_size) {
                    error(lty->loc, "bit size ", nums(*bit_size), " is too large")
                        .error(ity->loc, "for this")
                        .report();
                }
                b = a;  // fitting
            };
            if (left->node_type == ast::NodeType::int_type &&
                right->node_type == ast::NodeType::int_literal_type) {
                fitting(left, right);
            }
            else if (left->node_type == ast::NodeType::int_literal_type &&
                     right->node_type == ast::NodeType::int_type) {
                fitting(right, left);
            }
            else if (left->node_type == ast::NodeType::int_literal_type &&
                     right->node_type == ast::NodeType::int_literal_type) {
                left = assigning_type(left);
                right = assigning_type(right);
                auto li = ast::as<ast::IntType>(left);
                auto ri = ast::as<ast::IntType>(right);
                if (li->bit_size > ri->bit_size) {
                    right = left;
                }
                else {
                    left = right;
                }
            }
        }

        void typing_assign(ast::Binary* b) {
            auto right = b->right;
            if (!right->expr_type) {
                warn_not_typed(right);
                return;  // not typed yet
            }
            if (auto m = ast::as<ast::MemberAccess>(b->left)) {
                if (!m->expr_type) {
                    warn_not_typed(m);
                    return;  // not typed yet
                }
                if (!equal_type(m->expr_type, right->expr_type)) {
                    report_not_equal_type(m->expr_type, right->expr_type);
                }
                return;
            }
            auto left = ast::as<ast::Ident>(b->left);
            b->expr_type = void_type(b->loc);
            auto base = left->base.lock();
            auto base_ident = ast::as<ast::Ident>(base);
            auto report_assign_error = [&] {
                error(b->loc, "cannot assign to ", left->ident).error(base->loc, "ident ", left->ident, " is defined here").report();
            };
            auto new_type = assigning_type(right->expr_type);
            if (b->op == ast::BinaryOp::assign) {
                if (left->usage == ast::IdentUsage::unknown) {
                    error(left->loc, "identifier ", left->ident, " is not defined before; use := to define identifier").report();
                }
                else {
                    if (base_ident->usage == ast::IdentUsage::define_variable) {
                        if (!equal_type(base_ident->expr_type, new_type)) {
                            report_assign_error();
                        }
                    }
                    else if (base_ident->usage == ast::IdentUsage::define_const) {
                        report_assign_error();
                    }
                }
            }
            else if (b->op == ast::BinaryOp::define_assign) {
                if (left->usage == ast::IdentUsage::unknown) {
                    left->usage = ast::IdentUsage::define_variable;
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

        std::shared_ptr<ast::Type> extract_then_type(const std::shared_ptr<ast::IndentScope>& block) {
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
            if (if_->cond->expr_type) {
                check_bool(if_->cond.get());
            }
            typing_object(if_->then);
            if (if_->els) {
                typing_object(if_->els);
            }

            auto then_ = extract_then_type(if_->then);
            auto els_ = extract_else_type(if_->els);

            if (!then_ || !els_ ||
                (int_type_fitting(then_, els_), !equal_type(then_, els_))) {
                if_->expr_type = void_type(if_->loc);
                return;
            }

            if_->expr_type = then_;

            auto& then_ref = if_->then->elements.back();

            then_ref = std::make_shared<ast::ImplicitYield>(ast::cast_to<ast::Expr>(then_ref));
            if (auto block = ast::as<ast::IndentScope>(if_->els)) {
                auto& else_ref = block->elements.back();

                else_ref = std::make_shared<ast::ImplicitYield>(ast::cast_to<ast::Expr>(else_ref));
            }
        }

        // match has 2 pattern
        // 1. match expr:
        //       cond1 => stmt1
        //       cond2 => stmt2
        // then type of expr and cond should match
        // if stmt is expr statement then type of match is type of stmt
        // otherwise type of match is void
        // 2. match:
        //       cond1 => stmt1
        //       cond2 => stmt2
        // then type of cond should be bool
        // if stmt is expr statement then type of match is type of stmt
        // otherwise type of match is void
        void typing_match(ast::Match* m) {
            if (m->cond) {
                typing_expr(m->cond);
            }
            std::shared_ptr<ast::Type> candidate;
            for (auto& c : m->branch) {
                typing_expr(c->cond);
                if (c->cond->expr_type) {
                    if (m->cond) {
                        if (m->cond->expr_type) {
                            int_type_fitting(m->cond->expr_type, c->cond->expr_type);
                            if (!equal_type(m->cond->expr_type, c->cond->expr_type)) {
                                report_not_equal_type(m->cond->expr_type, c->cond->expr_type);
                            }
                        }
                    }
                    else {
                        check_bool(c->cond.get());
                    }
                }
                typing_object(c->then);
                if (auto exp = ast::as<ast::Expr>(c->then)) {
                    if (!candidate) {
                        candidate = exp->expr_type;
                    }
                    else {
                        if (candidate->node_type != ast::NodeType::void_type) {
                            int_type_fitting(candidate, exp->expr_type);
                            if (!equal_type(candidate, exp->expr_type)) {
                                candidate = void_type(m->loc);
                            }
                        }
                    }
                }
            }
            m->expr_type = candidate ? candidate : void_type(m->loc);
        }

        void typing_binary(ast::Binary* b) {
            auto op = b->op;
            auto& lty = b->left->expr_type;
            auto& rty = b->right->expr_type;
            auto report_binary_error = [&] {
                error(b->loc, "binary op ", *ast::bin_op_str(b->op), " is not valid")
                    .report();
            };
            if (!lty || !rty) {
                warn_not_typed(b);
                return;  // not typed yet
            }
            int_type_fitting(lty, rty);
            switch (op) {
                case ast::BinaryOp::left_logical_shift:
                case ast::BinaryOp::right_logical_shift:
                case ast::BinaryOp::add:
                case ast::BinaryOp::sub:
                case ast::BinaryOp::mul:
                case ast::BinaryOp::div:
                case ast::BinaryOp::mod: {
                    if (lty->node_type == ast::NodeType::int_type &&
                        rty->node_type == ast::NodeType::int_type) {
                        b->expr_type = std::move(lty);
                        return;
                    }
                    report_binary_error();
                }
                case ast::BinaryOp::bit_and:
                case ast::BinaryOp::bit_or:
                case ast::BinaryOp::bit_xor: {
                    if (lty->node_type == ast::NodeType::int_type &&
                        rty->node_type == ast::NodeType::int_type) {
                        if (!equal_type(rty, lty)) {
                            report_not_equal_type(rty, lty);
                        }
                        b->expr_type = std::move(lty);
                        return;
                    }
                    report_binary_error();
                }
                case ast::BinaryOp::equal:
                case ast::BinaryOp::not_equal:
                case ast::BinaryOp::less:
                case ast::BinaryOp::less_or_eq:
                case ast::BinaryOp::grater:
                case ast::BinaryOp::grater_or_eq: {
                    if (!equal_type(lty, rty)) {
                        report_not_equal_type(lty, rty);
                    }
                    b->expr_type = std::make_shared<ast::BoolType>(b->loc);
                    return;
                }
                case ast::BinaryOp::logical_and:
                case ast::BinaryOp::logical_or: {
                    if (lty->node_type == ast::NodeType::bool_type &&
                        rty->node_type == ast::NodeType::bool_type) {
                        b->expr_type = std::make_shared<ast::BoolType>(b->loc);
                        return;
                    }
                    report_binary_error();
                }
                default: {
                    report_binary_error();
                }
            }
        }

        void typing_binary_expr(ast::Binary* bin) {
            auto op = bin->op;
            typing_expr(
                bin->left,
                op == ast::BinaryOp::define_assign || op == ast::BinaryOp::const_assign);
            typing_expr(bin->right);
            switch (op) {
                case ast::BinaryOp::assign:
                case ast::BinaryOp::define_assign:
                case ast::BinaryOp::const_assign:
                    typing_assign(bin);
                    break;
                default:
                    typing_binary(bin);
            }
        }

        std::optional<std::shared_ptr<ast::Ident>> find_matching_ident(ast::Ident* ident) {
            auto search = [&](std::shared_ptr<ast::Ident>& def) {
                if (ident->ident == def->ident && def->usage != ast::IdentUsage::unknown) {
                    return true;
                }
                return false;
            };
            auto found = ident->scope->lookup_local<ast::Ident>(search);
            if (found) {
                return found;
            }
            return current_global->lookup_global<ast::Ident>(search);
        }

        std::optional<std::shared_ptr<ast::Format>> find_matching_fmt(ast::IdentType* ident) {
            auto search = [&](std::shared_ptr<ast::Format>& def) {
                if (def->ident->ident == ident->ident->ident) {
                    return true;
                }
                return false;
            };
            auto found = ident->ident->scope->lookup_local<ast::Format>(search);
            if (found) {
                return found;
            }
            return current_global->lookup_global<ast::Format>(search);
        }

        std::optional<std::shared_ptr<ast::Format>> find_matching_enum(ast::Ident* ident) {
            auto search = [&](std::shared_ptr<ast::Format>& def) {
                if (def->is_enum && def->ident->ident == ident->ident) {
                    return true;
                }
                return false;
            };
            auto found = ident->scope->lookup_local<ast::Format>(search);
            if (found) {
                return found;
            }
            return current_global->lookup_global<ast::Format>(search);
        }

        void typing_expr(const std::shared_ptr<ast::Expr>& expr, bool on_define = false) {
            if (auto lit = ast::as<ast::IntLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::IntLiteralType>(ast::cast_to<ast::IntLiteral>(expr));
            }
            else if (auto lit = ast::as<ast::BoolLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::BoolType>(lit->loc);
            }
            else if (auto lit = ast::as<ast::StrLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::StrLiteralType>(ast::cast_to<ast::StrLiteral>(expr));
            }
            else if (auto ident = ast::as<ast::Ident>(expr)) {
                if (ident->usage != ast::IdentUsage::unknown) {
                    return;  // skip
                }
                if (auto found = find_matching_ident(ident)) {
                    ident->expr_type = (*found)->expr_type;
                    ident->base = *found;
                    ident->usage = ast::IdentUsage::reference;
                }
                else if (auto found = find_matching_enum(ident)) {
                    ident->expr_type = (*found)->struct_type;
                    ident->base = *found;
                    ident->usage = ast::IdentUsage::reference_type;
                }
                else if (!on_define) {
                    warn_not_typed(ident);
                }
            }
            else if (auto bin = ast::as<ast::Binary>(expr)) {
                typing_binary_expr(bin);
            }
            else if (auto if_ = ast::as<ast::If>(expr)) {
                typing_if(if_);
            }
            else if (auto match = ast::as<ast::Match>(expr)) {
                typing_match(match);
            }
            else if (auto cond = ast::as<ast::Cond>(expr)) {
                typing_expr(cond->cond);
                typing_expr(cond->then);
                typing_expr(cond->els);
                if (cond->cond->expr_type) {
                    check_bool(cond->cond.get());
                }
                // even if cond->cond is not typed, we still need to type then and else
                if (!cond->then->expr_type || !cond->els->expr_type) {
                    warn_not_typed(cond);
                    return;  // not typed yet
                }
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
                typing_expr(unary->expr);
                switch (unary->op) {
                    case ast::UnaryOp::minus_sign:
                    case ast::UnaryOp::not_: {
                        unary->expr_type = unary->expr->expr_type;
                        break;
                    }
                    default: {
                        unsupported(expr);
                    }
                }
            }
            else if (auto call = ast::as<ast::Call>(expr)) {
                typing_expr(call->callee);
                if (!call->callee->expr_type) {
                    warn_not_typed(call);
                    // anyway, we need to type arguments
                    for (auto& arg : call->arguments) {
                        typing_expr(arg);
                    }
                    return;  // not typed yet
                }
                auto type = ast::as<ast::FunctionType>(call->callee->expr_type);
                if (!type) {
                    error(call->callee->loc, "expect function type but not").report();
                }
                if (call->arguments.size() != type->parameters.size()) {
                    error(call->loc, "expect ", nums(type->parameters.size()), " arguments but got ", nums(call->arguments.size()))
                        .error(type->loc, "function is defined here")
                        .report();
                }
                for (size_t i = 0; i < call->arguments.size(); i++) {
                    typing_expr(call->arguments[i]);
                    if (!call->arguments[i]->expr_type) {
                        warn_not_typed(call);
                        continue;  // not typed yet
                    }
                    int_type_fitting(call->arguments[i]->expr_type, type->parameters[i]);
                    if (!equal_type(call->arguments[i]->expr_type, type->parameters[i])) {
                        report_not_equal_type(call->arguments[i]->expr_type, type->parameters[i]);
                    }
                }
            }
            else if (auto selector = ast::as<ast::MemberAccess>(expr)) {
                typing_expr(selector->target);
                if (!selector->target->expr_type) {
                    warn_not_typed(selector);
                    return;
                }
                auto type = ast::as<ast::StructType>(selector->target->expr_type);
                if (!type) {
                    error(selector->target->loc, "expect struct type but not").report();
                }
                auto stmt = type->lookup(selector->member);
                if (!stmt) {
                    error(selector->loc, "member ", selector->member, " is not defined").report();
                }
                if (auto field = ast::as<ast::Field>(stmt)) {
                    selector->expr_type = field->field_type;
                }
                else if (auto fn = ast::as<ast::Function>(stmt)) {
                    selector->expr_type = fn->func_type;
                }
                else {
                    error(selector->loc, "member ", selector->member, " is not a field or function").report();
                }
            }
            else if (auto idx = ast::as<ast::Index>(expr)) {
                typing_expr(idx->expr);
                typing_expr(idx->index);
                if (!idx->expr->expr_type || !idx->index->expr_type) {
                    warn_not_typed(idx);
                    return;
                }
                unsupported(expr);
            }
            else if (ast::as<ast::Input>(expr) ||
                     ast::as<ast::Output>(expr) ||
                     ast::as<ast::Config>(expr)) {
                // typing already done
            }
            else if (auto r = ast::as<ast::Range>(expr)) {
                warn_not_typed(r);
            }
            else if (auto i = ast::as<ast::Import>(expr)) {
            }
            else {
                unsupported(expr);
            }
        }

        void typing_object(const std::shared_ptr<ast::Node>& ty) {
            // Define a lambda function for recursive traversal and typing
            auto recursive_typing = [&](auto&& f, const std::shared_ptr<ast::Node>& ty) -> void {
                auto do_traverse = [&] {
                    // Traverse the object's subcomponents and apply the recursive function
                    ast::traverse(ty, [&](const std::shared_ptr<ast::Node>& sub_ty) {
                        f(f, sub_ty);
                    });
                };
                if (auto expr = ast::as<ast::Expr>(ty)) {
                    // If the object is an expression, perform expression typing
                    typing_expr(ast::cast_to<ast::Expr>(ty));
                    return;
                }
                if (auto p = ast::as<ast::Program>(ty)) {
                    auto tmp = current_global;
                    current_global = p->global_scope;
                    const auto d = utils::helper::defer([&] {
                        current_global = std::move(tmp);
                    });
                    do_traverse();
                    return;
                }
                do_traverse();
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

}  // namespace brgen::middle
