/*license*/
#pragma once
#include "../lexer/token.h"
#include "../common/file.h"
#include <core/ast/traverse.h>
#include <helper/defer.h>

namespace brgen::middle {

    struct Typing {
        LocationError warnings;

        std::shared_ptr<ast::Scope> current_global;
        bool equal_type(const std::shared_ptr<ast::Type>& left, const std::shared_ptr<ast::Type>& right) {
            // unwrap ident type
            if (auto l = ast::as<ast::IdentType>(left), r = ast::as<ast::IdentType>(right); l || r) {
                if (l && r) {
                    return equal_type(l->base.lock(), r->base.lock());
                }
                if (l) {
                    return equal_type(l->base.lock(), right);
                }
                return equal_type(left, r->base.lock());
            }
            if (left->node_type != right->node_type) {
                return false;
            }
            if (auto lty = ast::as<ast::IntType>(left)) {
                auto rty = ast::as<ast::IntType>(right);
                return lty->bit_size == rty->bit_size;
            }

            if (auto lty = ast::as<ast::EnumType>(left)) {
                auto rty = ast::as<ast::EnumType>(right);
                return lty->base.lock() == rty->base.lock();
            }
            if (ast::as<ast::BoolType>(left)) {
                return true;
            }
            return false;
        }

        bool comparable_type(std::shared_ptr<ast::Type>& left, std::shared_ptr<ast::Type>& right) {
            if (equal_type(left, right)) {
                return true;
            }
            if (left->node_type == ast::NodeType::range_type) {
                auto rty = ast::as<ast::RangeType>(left);
                if (!rty->base_type) {
                    return true;  // range .. or ..= is always comparable to any type
                }
                int_type_fitting(rty->base_type, right);
                return equal_type(rty->base_type, right);
            }
            if (right->node_type == ast::NodeType::range_type) {
                auto rty = ast::as<ast::RangeType>(right);
                if (!rty->base_type) {
                    return true;  // range .. or ..= is always comparable to any type
                }
                int_type_fitting(rty->base_type, left);
                return equal_type(rty->base_type, left);
            }
            if (auto arr = ast::as<ast::ArrayType>(right)) {
                auto ty = ast::as<ast::IntType>(arr->base_type);
                if (!ty || ty->bit_size != 8) {
                    return false;  // only byte array is comparable with string
                }
                auto str = ast::as<ast::StrLiteralType>(left);
                if (!str) {
                    return false;
                }
                // TODO(on-keyday): check string literal length
                return true;
            }
            if (auto arr = ast::as<ast::ArrayType>(left)) {
                auto ty = ast::as<ast::IntType>(arr->base_type);
                if (!ty || ty->bit_size != 8) {
                    return false;  // only byte array is comparable with string
                }
                auto str = ast::as<ast::StrLiteralType>(right);
                if (!str) {
                    return false;
                }
                // TODO(on-keyday): check string literal length
                return true;
            }
            return false;
        }

        std::shared_ptr<ast::Type> common_type(std::shared_ptr<ast::Type>& a, std::shared_ptr<ast::Type>& b) {
            int_type_fitting(a, b);
            if (equal_type(a, b)) {
                return a;
            }
            if (a->node_type == ast::NodeType::int_type && b->node_type == ast::NodeType::int_type) {
                auto a_i = ast::as<ast::IntType>(a);
                auto b_i = ast::as<ast::IntType>(b);
                if (a_i->bit_size == b_i->bit_size) {
                    if (a_i->is_signed == b_i->is_signed) {
                        return a;
                    }
                    return a_i->is_signed ? b : a;
                }
                if (a_i->bit_size > b_i->bit_size) {
                    return a;
                }
                return b;
            }
            if (auto a_u = ast::as<ast::UnionType>(a)) {
                if (!a_u->common_type) {
                    return nullptr;
                }
                return common_type(a_u->common_type, b);
            }
            if (auto b_u = ast::as<ast::UnionType>(b)) {
                if (!b_u->common_type) {
                    return nullptr;
                }
                return common_type(a, b_u->common_type);
            }
            return nullptr;
        }

       private:
        auto void_type(lexer::Loc loc) {
            return std::make_shared<ast::VoidType>(loc);
        }

        [[noreturn]] void report_not_equal_type(const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            error(lty->loc, "type not equal here").error(rty->loc, "and here").report();
        }

        [[noreturn]] void report_not_comparable_type(const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            error(lty->loc, "type not comparable here").error(rty->loc, "and here").report();
        }

        [[noreturn]] void report_not_have_common_type(const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            error(lty->loc, "type not have common type here").error(rty->loc, "and here").report();
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

        void warn_type_not_found(auto&& expr) {
            warnings.warning(expr->loc, "type is not found");
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
                return std::make_shared<ast::IntType>(ty->loc, aligned, ast::Endian::unspec, false);
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

        void typing_assign(const std::shared_ptr<ast::Binary>& b) {
            auto right = b->right;
            auto check_right_typed = [&] {
                if (!right->expr_type) {
                    warn_not_typed(right);
                    return false;
                }
                return true;
            };
            if (auto m = ast::as<ast::MemberAccess>(b->left)) {
                if (!m->expr_type) {
                    warn_not_typed(m);
                    return;  // not typed yet
                }
                if (!check_right_typed()) {
                    return;
                }
                if (!equal_type(m->expr_type, right->expr_type)) {
                    report_not_equal_type(m->expr_type, right->expr_type);
                }
                return;
            }
            b->expr_type = void_type(b->loc);
            std::string ident;
            ast::Ident* left_ident = nullptr;
            ast::Ident* base_ident = nullptr;
            if (auto i_ = ast::as<ast::Ident>(b->left)) {
                left_ident = i_;
                ident = i_->ident;
                if (auto base = ast::as<ast::Ident>(i_->base.lock())) {
                    base_ident = base;
                }
            }
            else if (ast::as<ast::Input>(b->left)) {
                ident = "input";
            }
            else if (ast::as<ast::Output>(b->left)) {
                ident = "output";
            }
            else if (ast::as<ast::Config>(b->left)) {
                ident = "config";
            }
            else {
                unsupported(b->left);
            }
            auto report_assign_error = [&] {
                auto r = error(b->loc, "cannot assign to ", ident);
                if (base_ident) {
                    (void)r.error(base_ident->loc, "ident ", ident, " is defined here");
                }
                r.report();
            };
            if (!check_right_typed()) {
                return;
            }
            auto new_type = assigning_type(right->expr_type);
            if (b->op == ast::BinaryOp::assign) {
                if (left_ident && left_ident->usage == ast::IdentUsage::unknown) {
                    error(left_ident->loc, "identifier ", left_ident->ident, " is not defined before; use := to define identifier").report();
                }
                else {
                    if (!left_ident) {
                        warn_not_typed(b->left);
                        return;  // not typed yet
                    }
                    assert(base_ident);
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
                assert(left_ident);
                if (left_ident->usage == ast::IdentUsage::unknown) {
                    left_ident->usage = ast::IdentUsage::define_variable;
                    left_ident->expr_type = std::move(new_type);
                    left_ident->base = b;
                    left_ident->constant_level = ast::ConstantLevel::variable;
                }
                else {
                    report_assign_error();
                }
            }
            else if (b->op == ast::BinaryOp::const_assign) {
                assert(left_ident);
                if (left_ident->usage == ast::IdentUsage::unknown) {
                    left_ident->usage = ast::IdentUsage::define_const;
                    left_ident->expr_type = std::move(new_type);
                    left_ident->base = b;
                    if (b->right->constant_level == ast::ConstantLevel::const_value) {
                        left_ident->constant_level = ast::ConstantLevel::const_value;
                    }
                    else {
                        left_ident->constant_level = ast::ConstantLevel::const_variable;
                    }
                }
                else {
                    report_assign_error();
                }
            }
        }

        std::shared_ptr<ast::Type> extract_then_type(const std::shared_ptr<ast::IndentBlock>& block) {
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

            if (auto block = ast::as<ast::IndentBlock>(els)) {
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
            if (auto block = ast::as<ast::IndentBlock>(if_->els)) {
                auto& else_ref = block->elements.back();

                else_ref = std::make_shared<ast::ImplicitYield>(ast::cast_to<ast::Expr>(else_ref));
            }
            // TODO(on-keyday): analyze branch to decide actual constant level
            if_->constant_level = ast::ConstantLevel::variable;
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
            for (auto& bc : m->branch) {
                auto c = ast::as<ast::MatchBranch>(bc);
                if (!c) {
                    // maybe comments
                    continue;
                }
                typing_expr(c->cond);
                if (c->cond->expr_type) {
                    if (m->cond) {
                        if (m->cond->expr_type) {
                            int_type_fitting(m->cond->expr_type, c->cond->expr_type);
                            if (!comparable_type(m->cond->expr_type, c->cond->expr_type)) {
                                report_not_comparable_type(m->cond->expr_type, c->cond->expr_type);
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
            // TODO(on-keyday): analyze match branch to decide actual constant level
            m->constant_level = ast::ConstantLevel::variable;
        }

        auto decide_constant_level(ast::ConstantLevel a, ast::ConstantLevel b) {
            if (a == ast::ConstantLevel::unknown || b == ast::ConstantLevel::unknown) {
                return ast::ConstantLevel::unknown;
            }
            if (a == ast::ConstantLevel::const_value && b == ast::ConstantLevel::const_value) {
                return ast::ConstantLevel::const_value;
            }
            if (a == ast::ConstantLevel::variable || b == ast::ConstantLevel::variable) {
                return ast::ConstantLevel::variable;
            }
            assert(a == ast::ConstantLevel::const_variable ||
                   b == ast::ConstantLevel::const_variable);
            return ast::ConstantLevel::const_variable;
        }

        void typing_binary(const std::shared_ptr<ast::Binary>& b) {
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
            b->constant_level = decide_constant_level(b->left->constant_level, b->right->constant_level);
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
                    if (!comparable_type(lty, rty)) {
                        report_not_comparable_type(lty, rty);
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

        void typing_binary_expr(const std::shared_ptr<ast::Binary>& bin) {
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
                return ident->ident == def->ident && def->usage != ast::IdentUsage::unknown &&
                       def->usage != ast::IdentUsage::reference &&
                       def->usage != ast::IdentUsage::reference_type &&
                       def->usage != ast::IdentUsage::maybe_type &&
                       def->usage != ast::IdentUsage::reference_member;
            };
            auto found = ident->scope->lookup_local(search, ident);
            if (found) {
                return found;
            }
            return current_global->lookup_global(search);
        }

        std::shared_ptr<ast::StructType> lookup_struct(const std::shared_ptr<ast::Type>& typ) {
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                if (auto b = ident->base.lock()) {
                    if (auto fmt = ast::as<ast::StructType>(b)) {
                        return ast::cast_to<ast::StructType>(b);
                    }
                }
            }
            else if (ast::as<ast::StructType>(typ)) {
                return ast::cast_to<ast::StructType>(typ);
            }
            return nullptr;
        }

        std::shared_ptr<ast::EnumType> lookup_enum(const std::shared_ptr<ast::Type>& typ) {
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                if (auto fmt = ident->base.lock()) {
                    if (auto e = ast::as<ast::EnumType>(fmt)) {
                        return ast::cast_to<ast::EnumType>(fmt);
                    }
                }
            }
            else if (ast::as<ast::EnumType>(typ)) {
                return ast::cast_to<ast::EnumType>(typ);
            }
            return nullptr;
        }

        void typing_cond(ast::Cond* cond) {
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
            auto ty = common_type(lty, rty);
            if (!ty) {
                report_not_have_common_type(lty, rty);
            }
            cond->expr_type = ty;
            cond->constant_level = decide_constant_level(cond->then->constant_level, cond->els->constant_level);
        }

        void typing_call(ast::Call* call) {
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
            call->expr_type = type->return_type;
            // TODO(on-keyday): in the future, we may need to analyze function body to decide actual constant level
            call->constant_level = ast::ConstantLevel::variable;
        }

        void typing_member_access(ast::MemberAccess* selector) {
            typing_expr(selector->target);
            if (auto ident = ast::as<ast::Ident>(selector->target); ident && ident->usage == ast::IdentUsage::reference_type) {
                auto base = ident->base.lock();
                if (auto def = ast::as<ast::Ident>(base); def && def->usage == ast::IdentUsage::define_enum) {
                    base = def->base.lock();
                    if (auto enum_ = ast::as<ast::Enum>(base)) {
                        selector->expr_type = enum_->enum_type;
                        auto member = enum_->lookup(selector->member->ident);
                        if (!member) {
                            auto r = error(selector->member->loc, "member of enum ", ident->ident, ".", selector->member->ident, " is not defined");
                            r.error(ident->loc, "enum ", ident->ident, " is defined here").report();
                        }
                        selector->base = member->ident;
                        return;
                    }
                }
            }
            if (!selector->target->expr_type) {
                warn_not_typed(selector);
                return;
            }
            auto type = lookup_struct(selector->target->expr_type);
            if (!type) {
                error(selector->target->loc, "expect struct type but not").report();
            }
            auto stmt = type->lookup(selector->member->ident);
            if (!stmt) {
                error(selector->member->loc, "member ", selector->member->ident, " is not defined").report();
            }
            if (auto field = ast::as<ast::Field>(stmt)) {
                selector->expr_type = field->field_type;
                selector->base = stmt->ident;
            }
            else if (auto fn = ast::as<ast::Function>(stmt)) {
                selector->expr_type = fn->func_type;
                selector->base = stmt->ident;
            }
            else {
                auto r = error(selector->member->loc, "member ", selector->member->ident, " is not a field or function");
                (void)r.error(stmt->ident->loc, "member ", selector->member->ident, " is defined here");
                r.report();
            }
        }

        void typing_range(const std::shared_ptr<ast::Range>& r) {
            if (r->start) {
                typing_expr(r->start);
            }
            if (r->end) {
                typing_expr(r->end);
            }
            if (r->start && r->end) {
                if (!r->start->expr_type || !r->end->expr_type) {
                    warn_not_typed(r);
                    return;
                }
                int_type_fitting(r->start->expr_type, r->end->expr_type);
                if (!equal_type(r->start->expr_type, r->end->expr_type)) {
                    report_not_equal_type(r->start->expr_type, r->end->expr_type);
                }
            }
            auto range_type = std::make_shared<ast::RangeType>(r->loc);
            if (r->start) {
                range_type->base_type = r->start->expr_type;
            }
            else if (r->end) {
                range_type->base_type = r->end->expr_type;
            }
            range_type->range = r;
            r->expr_type = range_type;
            r->constant_level = decide_constant_level(r->start ? r->start->constant_level : ast::ConstantLevel::const_value,
                                                      r->end ? r->end->constant_level : ast::ConstantLevel::const_value);
        }

        void typing_ident(ast::Ident* ident) {
            if (ident->base.lock()) {
                assert(ident->usage != ast::IdentUsage::unknown);
                return;  // skip
            }
            if (auto found = find_matching_ident(ident)) {
                auto& base = (*found);
                ident->expr_type = base->expr_type;
                ident->base = base;
                ident->constant_level = base->constant_level;
                if (base->usage == ast::IdentUsage::define_enum ||
                    base->usage == ast::IdentUsage::define_format ||
                    base->usage == ast::IdentUsage::define_state) {
                    ident->usage = ast::IdentUsage::reference_type;
                }
                else {
                    ident->usage = ast::IdentUsage::reference;
                }
            }
            else {
                warn_not_typed(ident);
            }
        }

        void typing_expr(const std::shared_ptr<ast::Expr>& expr, bool on_define = false) {
            // treat cast as a special case
            // Cast has already been typed in the previous pass
            // but inner expression may not be typed
            if (auto c = ast::as<ast::Cast>(expr)) {
                typing_expr(c->expr);
            }
            if (expr->expr_type) {
                typing_object(expr->expr_type);
                return;  // already typed
            }
            if (auto lit = ast::as<ast::IntLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::IntLiteralType>(ast::cast_to<ast::IntLiteral>(expr));
                lit->constant_level = ast::ConstantLevel::const_value;
            }
            else if (auto lit = ast::as<ast::BoolLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::BoolType>(lit->loc);
                lit->constant_level = ast::ConstantLevel::const_value;
            }
            else if (auto lit = ast::as<ast::StrLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::StrLiteralType>(ast::cast_to<ast::StrLiteral>(expr));
                lit->constant_level = ast::ConstantLevel::const_value;
            }
            else if (auto ident = ast::as<ast::Ident>(expr)) {
                if (ident->base.lock()) {
                    assert(ident->usage != ast::IdentUsage::unknown);
                    return;  // skip
                }
                if (auto found = find_matching_ident(ident)) {
                    auto& base = (*found);
                    ident->expr_type = base->expr_type;
                    ident->base = base;
                    ident->constant_level = base->constant_level;
                    if (base->usage == ast::IdentUsage::define_enum ||
                        base->usage == ast::IdentUsage::define_format ||
                        base->usage == ast::IdentUsage::define_state) {
                        ident->usage = ast::IdentUsage::reference_type;
                    }
                    else {
                        ident->usage = ast::IdentUsage::reference;
                    }
                }
                else {
                    if (ident->usage == ast::IdentUsage::maybe_type) {
                    }
                    else if (!on_define) {
                        warn_not_typed(ident);
                    }
                }
            }
            else if (auto bin = ast::as<ast::Binary>(expr)) {
                typing_binary_expr(ast::cast_to<ast::Binary>(expr));
            }
            else if (auto if_ = ast::as<ast::If>(expr)) {
                typing_if(if_);
            }
            else if (auto match = ast::as<ast::Match>(expr)) {
                typing_match(match);
            }
            else if (auto cond = ast::as<ast::Cond>(expr)) {
                typing_cond(cond);
            }
            else if (auto paren = ast::as<ast::Paren>(expr)) {
                typing_expr(paren->expr);
                paren->expr_type = paren->expr->expr_type;
                paren->constant_level = paren->expr->constant_level;
            }
            else if (auto unary = ast::as<ast::Unary>(expr)) {
                typing_expr(unary->expr);
                switch (unary->op) {
                    case ast::UnaryOp::minus_sign:
                    case ast::UnaryOp::not_: {
                        unary->expr_type = unary->expr->expr_type;
                        unary->constant_level = unary->expr->constant_level;
                        break;
                    }
                    default: {
                        unsupported(expr);
                    }
                }
            }
            else if (auto call = ast::as<ast::Call>(expr)) {
                typing_call(call);
            }
            else if (auto selector = ast::as<ast::MemberAccess>(expr)) {
                typing_member_access(selector);
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
            else if (ast::as<ast::Range>(expr)) {
                typing_range(ast::cast_to<ast::Range>(expr));
            }
            else if (auto i = ast::as<ast::Import>(expr)) {
                expr->expr_type = i->import_desc->struct_type;
            }
            else {
                unsupported(expr);
            }
        }

        void typing_ident_type(ast::IdentType* s) {
            // If the object is an identifier, perform identifier typing
            typing_expr(s->ident);
            if (s->ident->usage == ast::IdentUsage::maybe_type) {
                warn_type_not_found(s->ident);
            }
            if (s->ident->usage != ast::IdentUsage::unknown &&
                s->ident->usage != ast::IdentUsage::reference_type &&
                s->ident->usage != ast::IdentUsage::maybe_type) {
                auto r = error(s->loc, "expect type name but not");
                if (auto b = s->ident->base.lock()) {
                    (void)r.error(b->loc, "identifier ", s->ident->ident, " is defined here");
                }
                r.report();
            }

            if (s->ident->usage == ast::IdentUsage::reference_type) {
                auto ident = ast::as<ast::Ident>(s->ident->base.lock());
                assert(ident);
                auto member = ast::as<ast::Member>(ident->base.lock());
                if (auto enum_ = ast::as<ast::Enum>(member)) {
                    s->base = enum_->enum_type;
                }
                else if (auto struct_ = ast::as<ast::Format>(member)) {
                    s->base = struct_->body->struct_type;
                }
                else if (auto state_ = ast::as<ast::State>(member)) {
                    error(s->loc, "state ", ident->ident, " is not usable for field type")
                        .error(member->ident->loc, "state ", ident->ident, " is defined here")
                        .report();
                }
            }
            return;
        }

        void typing_object(const std::shared_ptr<ast::Node>& ty) {
            // Define a lambda function for recursive traversal and typing
            auto recursive_typing = [&](auto&& f, const std::shared_ptr<ast::Node>& ty) -> void {
                auto do_traverse = [&] {
                    // Traverse the object's sub components and apply the recursive function
                    ast::traverse(ty, [&](const std::shared_ptr<ast::Node>& sub_ty) {
                        f(f, sub_ty);
                    });
                };
                if (auto expr = ast::as<ast::Expr>(ty)) {
                    // If the object is an expression, perform expression typing
                    typing_expr(ast::cast_to<ast::Expr>(ty));
                    return;
                }
                if (auto s = ast::as<ast::IdentType>(ty)) {
                    // If the object is an identifier, perform identifier typing
                    typing_ident_type(s);
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
                if (auto u = ast::as<ast::UnionType>(ty)) {
                    for (auto& c : u->candidates) {
                        auto f = c->field.lock();
                        if (!f) {
                            continue;
                        }
                        if (!u->common_type) {
                            u->common_type = f->field_type;
                        }
                        else {
                            u->common_type = common_type(u->common_type, f->field_type);
                            if (!u->common_type) {
                                break;
                            }
                        }
                    }
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
