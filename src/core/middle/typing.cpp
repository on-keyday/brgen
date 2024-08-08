/*license*/
#include "../lexer/token.h"
#include "../common/file.h"
#include <core/ast/traverse.h>
#include <helper/defer.h>
#include "replacer.h"
#include "../ast/tool/extract_config.h"
#include "../ast/tool/eval.h"
#include <core/ast/tool/compare.h>
#include <list>

namespace brgen::middle {

    struct Typing {
        LocationError& warnings;

        std::shared_ptr<ast::Scope> current_global;

        std::shared_ptr<ast::Type> unwrap_ident_type(const std::shared_ptr<ast::Type>& typ) {
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                return ident->base.lock();
            }
            return typ;
        }

        bool equal_type(const std::shared_ptr<ast::Type>& left, const std::shared_ptr<ast::Type>& right) {
            return ast::tool::equal_type(left, right);
        }

        bool comparable_type(std::shared_ptr<ast::Type>& left, std::shared_ptr<ast::Type>& right) {
            auto res = ast::tool::comparable_type(left, right);
            if (!res) {
                res.error().report();
            }
            return *res;
        }

        std::shared_ptr<ast::Type> common_type(std::shared_ptr<ast::Type>& a, std::shared_ptr<ast::Type>& b) {
            auto res = ast::tool::common_type(a, b);
            if (!res) {
                res.error().report();
            }
            return *res;
        }

        // OrCond_common_type is used for OrCond type inference
        // this function uses common_type first
        // and also infer common type between range and other types based on RangeType.base_type
        std::shared_ptr<ast::Type> OrCond_common_type(std::shared_ptr<ast::Type>& a, std::shared_ptr<ast::Type>& b) {
            auto res = ast::tool::OrCond_common_type(a, b);
            if (!res) {
                res.error().report();
            }
            return *res;
        }

       private:
        auto void_type(lexer::Loc loc) {
            return std::make_shared<ast::VoidType>(loc);
        }

        [[noreturn]] void report_not_equal_type(lexer::Loc loc, const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            auto l = ast::tool::type_to_string(lty);
            auto r = ast::tool::type_to_string(rty);
            error(loc, "type mismatch").error(lty->loc, "type not equal here ", l).error(rty->loc, "and here ", r).report();
        }

        [[noreturn]] void report_not_comparable_type(lexer::Loc loc, const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            auto l = ast::tool::type_to_string(lty);
            auto r = ast::tool::type_to_string(rty);
            error(loc, "type mismatch").error(lty->loc, "type not comparable here ", l).error(rty->loc, "and here ", r).report();
        }

        [[noreturn]] void report_not_have_common_type(const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            auto l = ast::tool::type_to_string(lty);
            auto r = ast::tool::type_to_string(rty);
            error(lty->loc, "type not have common type here ", l).error(rty->loc, "and here ", r).report();
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

        std::shared_ptr<ast::Type> int_literal_to_int_type(const std::shared_ptr<ast::Type>& base) {
            if (auto ty = ast::as<ast::IntLiteralType>(base)) {
                auto aligned = ty->get_aligned_bit();
                return std::make_shared<ast::IntType>(ty->loc, aligned, ast::Endian::unspec, false);
            }
            return base;
        }

        // int_type_fitting convert IntLiteralType to IntType to assign or compare
        void int_type_fitting(std::shared_ptr<ast::Type>& left, std::shared_ptr<ast::Type>& right) {
            auto fitting = [&](auto& a, auto& b) {
                auto ity = ast::as<ast::IntType>(a);
                auto lty = ast::as<ast::IntLiteralType>(b);
                auto bit_size = lty->bit_size;
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
                left = int_literal_to_int_type(left);
                right = int_literal_to_int_type(right);
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

            auto handle_member_or_index_access = [&](const auto& access_node) {
                if (!access_node->expr_type) {
                    warn_not_typed(access_node);
                    return false;
                }
                if (!check_right_typed()) {
                    return false;
                }
                int_type_fitting(access_node->expr_type, right->expr_type);
                if (!equal_type(access_node->expr_type, right->expr_type)) {
                    report_not_equal_type(access_node->loc, access_node->expr_type, right->expr_type);
                }
                return true;
            };

            if (auto m = ast::as<ast::MemberAccess>(b->left)) {
                handle_member_or_index_access(m);
                return;
            }
            if (auto idx = ast::as<ast::Index>(b->left)) {
                handle_member_or_index_access(idx);
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
            else if (auto sp = ast::as<ast::SpecialLiteral>(b->left)) {
                ident = to_string(sp->kind);
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
            if (b->op == ast::BinaryOp::define_assign) {
                assert(left_ident);
                assert(left_ident->usage == ast::IdentUsage::define_variable);
                assert(left_ident->base.lock() == b);
                left_ident->expr_type = int_literal_to_int_type(right->expr_type);
                left_ident->constant_level = ast::ConstantLevel::variable;
            }
            else if (b->op == ast::BinaryOp::const_assign) {
                assert(left_ident);
                assert(left_ident->usage == ast::IdentUsage::define_const);
                assert(left_ident->base.lock() == b);
                left_ident->expr_type = int_literal_to_int_type(right->expr_type);
                if (b->right->constant_level == ast::ConstantLevel::constant) {
                    left_ident->constant_level = ast::ConstantLevel::constant;
                }
                else {
                    left_ident->constant_level = ast::ConstantLevel::immutable_variable;
                }
            }
            else if (b->op == ast::BinaryOp::in_assign) {
                assert(left_ident);
                assert(left_ident->usage == ast::IdentUsage::define_variable);
                assert(left_ident->base.lock() == b);
                auto new_type = int_literal_to_int_type(right->expr_type);
                // `for x in 10`, x is int type inferred from 10
                if (auto p = ast::as<ast::IntType>(new_type)) {
                    left_ident->expr_type = std::move(new_type);
                    left_ident->usage = ast::IdentUsage::define_const;
                    left_ident->constant_level = ast::ConstantLevel::immutable_variable;
                }
                // `for x in "hello"`, x is u8 type
                else if (auto str = ast::as<ast::StrLiteralType>(new_type)) {
                    auto u8 = std::make_shared<ast::IntType>(str->loc, 8, ast::Endian::unspec, false);
                    left_ident->expr_type = std::move(u8);
                    left_ident->usage = ast::IdentUsage::define_const;
                    left_ident->constant_level = ast::ConstantLevel::immutable_variable;
                }
                // `for x in 1..10`, x is base_type of range
                else if (auto r = ast::as<ast::RangeType>(new_type)) {
                    left_ident->expr_type = r->base_type;
                    left_ident->usage = ast::IdentUsage::define_const;
                    left_ident->constant_level = ast::ConstantLevel::immutable_variable;
                }
                // `for x in array`, x is element type of array
                else if (auto arr = ast::as<ast::ArrayType>(new_type)) {
                    left_ident->expr_type = arr->element_type;
                    if (right->constant_level == ast::ConstantLevel::constant ||
                        right->constant_level == ast::ConstantLevel::immutable_variable) {
                        left_ident->usage = ast::IdentUsage::define_const;
                        left_ident->constant_level = ast::ConstantLevel::immutable_variable;
                    }
                    else {
                        left_ident->usage = ast::IdentUsage::define_variable;
                        left_ident->constant_level = ast::ConstantLevel::variable;
                    }
                }
                else {
                    error(b->loc, "cannot use ", ast::node_type_to_string(new_type->node_type), " in `for in` syntax; integer, string, range, and array are allowed").report();
                }
            }
            else {  // other assignments like =, +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=
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
                        int_type_fitting(base_ident->expr_type, right->expr_type);
                        if (!equal_type(base_ident->expr_type, right->expr_type)) {
                            report_assign_error();
                        }
                    }
                    else if (base_ident->usage == ast::IdentUsage::define_const) {
                        report_assign_error();
                    }
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
            typing_expr(if_->cond);
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

        template <class T>
        struct UnfilledRange {
            T start;
            T end;
        };

        template <class T>
        void check_filler(T min_value, T max_value, ast::Match* m) {
            std::list<UnfilledRange<T>> unfilled;
            unfilled.push_back({min_value, max_value});
            ast::tool::Evaluator eval;
            bool filled = false;
            T l_val = 0;
            T r_val = 0;
            auto update_fill = [&](auto l_val, auto r_val, bool inclusive) {
                for (auto it = unfilled.begin(); it != unfilled.end(); it++) {
                    auto& u = *it;
                    bool cond = false;
                    if (inclusive) {
                        cond = l_val >= u.start && r_val <= u.end;
                    }
                    else {
                        cond = l_val >= u.start && r_val < u.end;
                    }
                    if (cond) {
                        if (l_val > u.start) {
                            unfilled.insert(it, {u.start, l_val - 1});
                        }
                        if (r_val < u.end) {
                            unfilled.insert(it, {r_val + 1, u.end});
                        }
                        unfilled.erase(it, unfilled.end());
                        break;
                    }
                }
            };
            auto range_eval = [&](ast::Range* range) {
                auto l = eval.template eval_as<ast::tool::EResultType::integer>(range->start);
                if (range->start && !l) {
                    return false;  // not constant, cannot check exhaustiveness
                }
                auto r = eval.template eval_as<ast::tool::EResultType::integer>(range->end);
                if (range->end && !r) {
                    return false;  // not constant, cannot check exhaustiveness
                }
                if (!l) {
                    l->emplace<ast::tool::EResultType::integer>(min_value);
                }
                if (!r) {
                    r->emplace<ast::tool::EResultType::integer>(max_value);
                }
                l_val = T(l->get<ast::tool::EResultType::integer>());
                r_val = T(r->get<ast::tool::EResultType::integer>());
                if (range->op == ast::BinaryOp::range_inclusive
                        ? l_val > r_val
                        : l_val >= r_val) {
                    error(range->loc, "range start is greater than end").report();
                    return false;
                }
                update_fill(l_val, r_val, range->op == ast::BinaryOp::range_inclusive);
                return true;
            };
            auto single_expr_eval = [&](auto&& expr) {
                auto l = eval.template eval_as<ast::tool::EResultType::integer>(expr);
                if (!l) {
                    return false;  // not constant, cannot check exhaustiveness
                }
                l_val = T(l->template get<ast::tool::EResultType::integer>());
                r_val = l_val;
                update_fill(l_val, r_val, true);
                return true;
            };
            for (auto& b : m->branch) {
                if (filled) {
                    warnings.warning(b->loc, "maybe unreachable code");
                    continue;
                }
                if (auto or_cond = ast::as<ast::OrCond>(b->cond->expr)) {
                    for (auto& cond : or_cond->conds) {
                        if (auto range = ast::as<ast::Range>(cond)) {
                            if (!range_eval(range)) {
                                return;
                            }
                        }
                        else {
                            if (!single_expr_eval(cond)) {
                                return;
                            }
                        }
                    }
                }
                else if (auto range = ast::as<ast::Range>(b->cond->expr)) {
                    if (!range_eval(range)) {
                        return;
                    }
                }
                else {
                    if (!single_expr_eval(b->cond->expr)) {
                        return;
                    }
                }
                if (unfilled.empty()) {
                    m->struct_union_type->exhaustive = true;
                    filled = true;
                }
            }
        }

        void check_exhaustiveness(ast::Match* m) {
            auto typ = m->cond->expr_type;
            auto enum_ = ast::as<ast::EnumType>(typ);
            auto int_ = ast::as<ast::IntType>(typ);
            if (enum_) {
                auto base = enum_->base.lock()->base_type;
                int_ = ast::as<ast::IntType>(base);
            }
            if (!int_ && !enum_) {
                return;
            }
            if (int_) {
                if (int_->bit_size > 64) {
                    return;  // TODO(on-keyday): currently we don't support large int
                }
                auto max_value_base = ~(~std::uint64_t(0) << *int_->bit_size);
                if (int_->is_signed) {
                    max_value_base = max_value_base >> 1;
                    std::int64_t max_value = max_value_base;
                    std::int64_t min_value = -max_value - 1;
                    check_filler(min_value, max_value, m);
                }
                else {
                    std::uint64_t max_value = max_value_base;
                    std::uint64_t min_value = 0;
                    check_filler(min_value, max_value, m);
                }
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
            std::shared_ptr<ast::Expr> any_match;
            bool trial_match = false;
            auto report_any_match = [&](auto& c) {
                error(c->loc, "any match (`..`) must be unique and be end of in match branch")
                    .error(any_match->loc, "any match (`..`) is already defined here")
                    .report();
            };
            for (auto& c : m->branch) {
                // TODO(on-keyday): check exhaustiveness for range, mainly for enum and int
                if (any_match && !trial_match) {
                    if (m->cond) {
                        report_any_match(c);
                    }
                    trial_match = true;
                }
                typing_expr(c->cond);

                // check match branch condition type
                if (c->cond->expr_type) {
                    if (m->cond) {
                        if (m->cond->expr_type) {
                            int_type_fitting(m->cond->expr_type, c->cond->expr_type);
                            if (!comparable_type(m->cond->expr_type, c->cond->expr_type)) {
                                report_not_comparable_type(m->cond->loc, m->cond->expr_type, c->cond->expr_type);
                            }
                        }
                    }
                    else {
                        if (!ast::is_any_range(c->cond)) {
                            check_bool(c->cond.get());
                        }
                    }
                }
                typing_object(c->then);
                if (auto sc = ast::as<ast::ScopedStatement>(c->then)) {
                    if (auto exp = ast::as<ast::Expr>(sc->statement)) {
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
                if (ast::is_any_range(c->cond)) {
                    any_match = c->cond;
                }
                else if (trial_match) {
                    report_any_match(c);
                }
            }
            m->trial_match = trial_match;
            m->struct_union_type->exhaustive = any_match != nullptr;
            // if no any match, but maybe exhaustive, then we need to check exhaustiveness
            if (m->cond && !m->struct_union_type->exhaustive) {
                check_exhaustiveness(m);
            }
            m->expr_type = candidate ? candidate : void_type(m->loc);
            // TODO(on-keyday): analyze match branch to decide actual constant level
            m->constant_level = ast::ConstantLevel::variable;
        }

        auto decide_constant_level(ast::ConstantLevel a, ast::ConstantLevel b) {
            if (a == ast::ConstantLevel::unknown || b == ast::ConstantLevel::unknown) {
                return ast::ConstantLevel::unknown;
            }
            if (a == ast::ConstantLevel::constant && b == ast::ConstantLevel::constant) {
                return ast::ConstantLevel::constant;
            }
            if (a == ast::ConstantLevel::variable || b == ast::ConstantLevel::variable) {
                return ast::ConstantLevel::variable;
            }
            assert(a == ast::ConstantLevel::immutable_variable ||
                   b == ast::ConstantLevel::immutable_variable);
            return ast::ConstantLevel::immutable_variable;
        }

        void typing_binary(const std::shared_ptr<ast::Binary>& b) {
            auto op = b->op;
            auto& lty = b->left->expr_type;
            auto& rty = b->right->expr_type;
            if (!lty || !rty) {
                warn_not_typed(b);
                return;  // not typed yet
            }
            int_type_fitting(lty, rty);
            b->constant_level = decide_constant_level(b->left->constant_level, b->right->constant_level);
            auto cmp_lty = unwrap_ident_type(lty);
            auto cmp_rty = unwrap_ident_type(rty);
            if (!cmp_lty || !cmp_rty) {
                warn_type_not_found(b);
                return;
            }
            auto report_binary_error = [&] {
                error(b->loc, "binary op ", ast::to_string(b->op), " is not valid")
                    .error(b->left->loc, "left type is ", ast::node_type_to_string(cmp_lty->node_type))
                    .error(b->right->loc, "right type is ", ast::node_type_to_string(cmp_rty->node_type))
                    .report();
            };
            switch (op) {
                case ast::BinaryOp::left_logical_shift:
                case ast::BinaryOp::right_logical_shift:
                case ast::BinaryOp::add:
                case ast::BinaryOp::sub:
                case ast::BinaryOp::mul:
                case ast::BinaryOp::div:
                case ast::BinaryOp::mod: {
                    auto typ = common_type(cmp_lty, cmp_rty);
                    if (typ) {
                        b->expr_type = std::move(typ);
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
                            report_not_equal_type(b->loc, rty, lty);
                        }
                        b->expr_type = lty;
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
                        report_not_comparable_type(b->loc, lty, rty);
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
                case ast::BinaryOp::comma: {
                    b->expr_type = rty;
                    return;
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
                op == ast::BinaryOp::define_assign || op == ast::BinaryOp::const_assign ||
                    op == ast::BinaryOp::in_assign /*`for x in range*/);
            typing_expr(bin->right);
            switch (op) {
                case ast::BinaryOp::assign:
                case ast::BinaryOp::define_assign:
                case ast::BinaryOp::const_assign:
                case ast::BinaryOp::in_assign:
                case ast::BinaryOp::add_assign:
                case ast::BinaryOp::sub_assign:
                case ast::BinaryOp::mul_assign:
                case ast::BinaryOp::div_assign:
                case ast::BinaryOp::mod_assign:
                case ast::BinaryOp::left_logical_shift_assign:
                case ast::BinaryOp::right_logical_shift_assign:
                case ast::BinaryOp::left_arithmetic_shift_assign:
                case ast::BinaryOp::right_arithmetic_shift_assign:
                case ast::BinaryOp::bit_and_assign:
                case ast::BinaryOp::bit_or_assign:
                case ast::BinaryOp::bit_xor_assign:
                    typing_assign(bin);
                    break;
                default:
                    typing_binary(bin);
            }
        }

        void typing_unary(ast::Unary* unary) {
            typing_expr(unary->expr);
            switch (unary->op) {
                case ast::UnaryOp::minus_sign:
                case ast::UnaryOp::not_: {
                    unary->expr_type = unary->expr->expr_type;
                    unary->constant_level = unary->expr->constant_level;
                    break;
                }
                default: {
                    unsupported(unary);
                }
            }
        }

        void typing_paren(ast::Paren* paren) {
            typing_expr(paren->expr);
            paren->expr_type = paren->expr->expr_type;
            paren->constant_level = paren->expr->constant_level;
        }

        std::optional<std::shared_ptr<ast::Ident>> find_matching_ident(ast::Ident* ident) {
            auto search = [&](std::shared_ptr<ast::Ident>& def) {
                return ident != def.get() &&
                       ident->ident == def->ident &&
                       def->usage != ast::IdentUsage::unknown &&
                       def->usage != ast::IdentUsage::reference &&
                       def->usage != ast::IdentUsage::reference_type &&
                       def->usage != ast::IdentUsage::maybe_type &&
                       def->usage != ast::IdentUsage::reference_member &&
                       def->usage != ast::IdentUsage::reference_member_type;
            };
            auto found = ident->scope->lookup_local(search, ident);
            if (found) {
                return found;
            }
            return current_global->lookup_global(search);
        }

        std::shared_ptr<ast::UnionType> lookup_union(const std::shared_ptr<ast::Type>& typ) {
            if (ast::as<ast::UnionType>(typ)) {
                return ast::cast_to<ast::UnionType>(typ);
            }
            return nullptr;
        }

        std::shared_ptr<ast::StructType> lookup_struct(const std::shared_ptr<ast::Type>& typ) {
            auto t = typ;
            if (auto ident = ast::as<ast::IdentType>(t)) {
                auto b = ident->base.lock();
                if (!b) {
                    typing_ident_type(ident, true);
                    b = ident->base.lock();
                }
                t = b;
            }
            if (ast::as<ast::StructType>(t)) {
                return ast::cast_to<ast::StructType>(t);
            }
            return nullptr;
        }

        std::shared_ptr<ast::EnumType> lookup_enum(const std::shared_ptr<ast::Type>& typ) {
            auto t = typ;
            if (auto ident = ast::as<ast::IdentType>(t)) {
                auto b = ident->base.lock();
                if (!b) {
                    typing_ident_type(ident, true);
                    b = ident->base.lock();
                }
                t = b;
            }
            if (ast::as<ast::EnumType>(t)) {
                return ast::cast_to<ast::EnumType>(t);
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

        void call_to_cast(ast::Call* call, NodeReplacer base_node, ast::TypeLiteral* typ_lit) {
            // replace with cast node
            auto s = ast::as<ast::IdentType>(typ_lit->type_literal);
            assert(s);
            std::shared_ptr<ast::Node> base;
            lexer::Loc def_loc;
            if (s->import_ref) {
                auto def = s->import_ref->base.lock();
                base = def->base.lock();
                def_loc = def->loc;
            }
            else {
                auto def = s->ident->base.lock();
                assert(def);
                assert(def->node_type == ast::NodeType::ident);
                base = ast::as<ast::Ident>(def)->base.lock();
                def_loc = def->loc;
            }
            if (ast::as<ast::Enum>(base) || ast::as<ast::Format>(base)) {
                call->expr_type = typ_lit->type_literal;
            }
            else {
                error(call->callee->loc, "expect enum or format type but not")
                    .error(def_loc, "type is ", ast::node_type_to_string(base->node_type))
                    .report();
            }
            if (call->arguments.size() > 1) {
                error(call->loc, "expect 0 or 1 argument but got ", nums(call->arguments.size())).report();
            }
            auto copy = call->expr_type;
            assert(copy);
            auto cast = std::make_shared<ast::Cast>(ast::cast_to<ast::Call>(base_node.to_node()), std::move(copy),
                                                    call->arguments);
            base_node.replace(std::move(cast));
            for (auto& arg : call->arguments) {
                typing_expr(arg);
            }
        }

        void typing_call(ast::Call* call, NodeReplacer base_node) {
            typing_expr(call->callee);
            if (auto typ_lit = ast::as<ast::TypeLiteral>(call->callee)) {
                call_to_cast(call, base_node, typ_lit);
                return;
            }
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
                    report_not_equal_type(call->arguments[i]->loc, call->arguments[i]->expr_type, type->parameters[i]);
                }
            }
            call->expr_type = type->return_type;
            // TODO(on-keyday): in the future, we may need to analyze function body to decide actual constant level
            call->constant_level = ast::ConstantLevel::variable;
        }

        void typing_enum_member_access(ast::MemberAccess* selector, ast::TypeLiteral* tylit) {
            auto ty = ast::as<ast::IdentType>(tylit->type_literal);
            if (!ty) {
                error(selector->target->loc, "expect enum type but not")
                    .error(tylit->type_literal->loc, "type is ", ast::node_type_to_string(tylit->type_literal->node_type))
                    .report();
            }
            if (auto def = ast::as<ast::EnumType>(ty->base.lock())) {
                auto enum_ = def->base.lock();
                assert(enum_);
                selector->expr_type = enum_->enum_type;
                auto member = enum_->lookup(selector->member->ident);
                if (!member) {
                    auto r = error(selector->member->loc, "member of enum ", ty->ident->ident, ".", selector->member->ident, " is not defined");
                    r.error(tylit->loc, "enum ", enum_->ident->ident, " is defined here").report();
                }
                selector->base = member->ident;
                return;
            }
            else {
                error(selector->target->loc, "expect enum type but not")
                    .error(tylit->type_literal->loc, "type is ", ast::node_type_to_string(tylit->type_literal->node_type))
                    .report();
            }
        }

        void typing_index(ast::Index* idx) {
            typing_expr(idx->expr);
            typing_expr(idx->index);
            if (!idx->expr->expr_type || !idx->index->expr_type) {
                warn_not_typed(idx);
                return;
            }
            auto arr_ty = ast::as<ast::ArrayType>(idx->expr->expr_type);
            if (!arr_ty) {
                error(idx->expr->loc, "expect array type but not")
                    .error(idx->expr->expr_type->loc, "type is ", ast::node_type_to_string(idx->expr->expr_type->node_type))
                    .report();
            }
            idx->expr_type = arr_ty->element_type;
            idx->constant_level = decide_constant_level(idx->expr->constant_level, idx->index->constant_level);
        }

        void typing_builtin_member_access(ast::MemberAccess* selector) {
            if (auto arr = ast::as<ast::ArrayType>(selector->target->expr_type)) {
                if (selector->member->ident == "length") {
                    selector->member->usage = ast::IdentUsage::reference_builtin_fn;
                    selector->expr_type = std::make_shared<ast::IntType>(selector->loc, 64, ast::Endian::unspec, false);
                    selector->constant_level = arr->length_value ? ast::ConstantLevel::constant : ast::ConstantLevel::immutable_variable;
                    return;  // length is a builtin function of array
                }
                error(selector->member->loc, "member ", selector->member->ident, " is not usable for array; use `.length` instead")
                    .error(selector->target->loc, "type is ", ast::node_type_to_string(selector->target->expr_type->node_type))
                    .report();
            }
            error(selector->target->loc, "expect struct type but not")
                .error(selector->target->expr_type->loc, "type is ", ast::node_type_to_string(selector->target->expr_type->node_type))
                .report();
        }

        void typing_member_access(ast::MemberAccess* selector) {
            typing_expr(selector->target);
            if (auto tylit = ast::as<ast::TypeLiteral>(selector->target)) {
                typing_enum_member_access(selector, tylit);
                return;
            }
            if (!selector->target->expr_type) {
                warn_not_typed(selector);
                return;
            }
            std::shared_ptr<ast::Member> stmt;
            if (auto union_ = lookup_union(selector->target->expr_type)) {
                error(selector->target->loc, "union type is not supported yet").report();
            }
            else if (auto type = lookup_struct(selector->target->expr_type)) {
                stmt = type->lookup(selector->member->ident);
                if (!stmt) {
                    auto res = error(selector->member->loc, "member ", selector->member->ident, " is not defined");
                    if (auto f = ast::as<ast::Member>(type->base.lock())) {
                        (void)res.error(f->ident->loc, "type ", f->ident->ident, " is defined here");
                    }
                    res.report();
                }
                typing_object(stmt);
            }
            else {
                typing_builtin_member_access(selector);
                return;
            }
            if (auto field = ast::as<ast::Field>(stmt)) {
                selector->expr_type = field->field_type;
                selector->base = stmt->ident;
            }
            else if (auto fn = ast::as<ast::Function>(stmt)) {
                selector->expr_type = fn->func_type;
                selector->base = stmt->ident;
            }
            else if (auto fmt = ast::as<ast::Format>(stmt)) {
                selector->base = fmt->ident;
                selector->member->usage = ast::IdentUsage::reference_member_type;
            }
            else if (auto state_ = ast::as<ast::State>(stmt)) {
                selector->base = state_->ident;
                selector->member->usage = ast::IdentUsage::reference_member_type;
            }
            else if (auto enum_ = ast::as<ast::Enum>(stmt)) {
                selector->base = enum_->ident;
                selector->member->usage = ast::IdentUsage::reference_member_type;
            }
            else {
                error(selector->member->loc, "member ", selector->member->ident, " is not a field, function, format, state or enum")
                    .error(stmt->ident->loc, "member ", selector->member->ident, " is defined here")
                    .report();
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
                    report_not_equal_type(r->loc, r->start->expr_type, r->end->expr_type);
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
            r->constant_level = decide_constant_level(r->start ? r->start->constant_level : ast::ConstantLevel::constant,
                                                      r->end ? r->end->constant_level : ast::ConstantLevel::constant);
        }

        std::shared_ptr<ast::IdentType> get_final_ident_type(const std::shared_ptr<ast::Type>& typ) {
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                return ast::cast_to<ast::IdentType>(typ);
            }
            if (auto arr = ast::as<ast::ArrayType>(typ)) {
                return get_final_ident_type(arr->element_type);
            }
            return nullptr;
        }

        void register_state_variable(const std::shared_ptr<ast::Ident>& ident) {
            auto maybe_field = ast::as<ast::Ident>(ident->base.lock())->base.lock();
            auto field = ast::as<ast::Field>(maybe_field);
            assert(field);
            auto ident_typ = get_final_ident_type(field->field_type);
            if (!ident_typ) {
                return;  // not a state variable
            }
            typing_ident_type(ident_typ.get(), true);
            auto l = ast::as<ast::StructType>(ident_typ->base.lock());
            if (!l) {
                return;  // not a state variable
            }
            auto st = ast::as<ast::State>(l->base.lock());
            if (!st) {
                return;  // not a state variable
            }
            auto s = ident->scope;
            while (s) {
                auto o = s->owner.lock();
                if (auto fmt = ast::as<ast::Format>(o)) {
                    auto field = ast::cast_to<ast::Field>(maybe_field);
                    field->is_state_variable = true;
                    fmt->state_variables.push_back(std::move(field));
                    return;
                }
                s = s->prev.lock();
            }
        }

        void typing_ident(const std::shared_ptr<ast::Ident>& ident, bool on_define) {
            if (ident->base.lock()) {
                assert(ident->usage != ast::IdentUsage::unknown);
                return;  // skip
            }
            if (auto found = find_matching_ident(ident.get())) {
                auto& base = (*found);
                if (auto def = ast::as<ast::Binary>(base->base.lock());
                    def && !def->expr_type) {
                    auto bin = ast::cast_to<ast::Binary>(base->base.lock());
                    typing_expr(bin->right);
                    typing_assign(bin);
                }
                ident->expr_type = base->expr_type;
                assert([&] {auto l=base->base.lock();return l&&l->node_type != ast::NodeType::ident; }());
                ident->base = base;
                ident->constant_level = base->constant_level;
                if (base->usage == ast::IdentUsage::define_enum ||
                    base->usage == ast::IdentUsage::define_format ||
                    base->usage == ast::IdentUsage::define_state) {
                    assert(ident->expr_type == nullptr);
                    ident->usage = ast::IdentUsage::reference_type;
                }
                else {
                    ident->usage = ast::IdentUsage::reference;
                    if (base->usage == ast::IdentUsage::define_field) {
                        register_state_variable(ident);
                    }
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

        void typing_io_operation(ast::IOOperation* io) {
            switch (io->method) {
                case ast::IOMethod::unspec:
                    break;
                case ast::IOMethod::input_offset:
                case ast::IOMethod::input_remain:
                case ast::IOMethod::input_bit_offset:
                    io->expr_type = std::make_shared<ast::IntType>(io->loc, 64, ast::Endian::unspec, false);
                    io->constant_level = ast::ConstantLevel::immutable_variable;
                    break;
                case ast::IOMethod::output_put:
                case ast::IOMethod::input_backward: {
                    io->expr_type = void_type(io->loc);
                    io->constant_level = ast::ConstantLevel::variable;
                    auto c = ast::as<ast::Call>(io->base);
                    assert(c);
                    for (auto& arg : c->arguments) {
                        typing_expr(arg, false);
                        io->arguments.push_back(arg);
                    }
                    break;
                }
                case ast::IOMethod::input_get:
                case ast::IOMethod::input_peek: {
                    io->constant_level = ast::ConstantLevel::variable;
                    auto c = ast::as<ast::Call>(io->base);
                    assert(c);
                    if (c->arguments.size() > 2) {
                        error(io->loc, "expect 0, 1 or 2 arguments but got ", nums(c->arguments.size())).report();
                    }
                    for (auto& arg : c->arguments) {
                        typing_expr(arg, false);
                    }
                    if (c->arguments.size() >= 1) {
                        auto typ = ast::as<ast::TypeLiteral>(c->arguments[0]);
                        if (!typ) {
                            error(c->arguments[0]->loc, "expect type literal but not")
                                .error(c->arguments[0]->loc, "type is ", ast::node_type_to_string(c->arguments[0]->node_type))
                                .report();
                        }
                        io->expr_type = typ->type_literal;
                    }
                    if (c->arguments.size() >= 2) {
                        io->arguments.push_back(c->arguments[1]);
                        // TODO(on-keyday): check integer type?
                    }
                    if (io->expr_type == nullptr) {
                        io->expr_type = std::make_shared<ast::IntType>(io->loc, 8, ast::Endian::unspec, false);
                    }
                    break;
                }
                case ast::IOMethod::config_endian_big:
                case ast::IOMethod::config_endian_little:
                case ast::IOMethod::config_endian_native:
                case ast::IOMethod::config_bit_order_lsb:
                case ast::IOMethod::config_bit_order_msb: {
                    io->expr_type = std::make_shared<ast::BoolType>(io->loc);
                    io->constant_level = ast::ConstantLevel::constant;
                    break;
                }
                case ast::IOMethod::input_subrange: {
                    io->constant_level = ast::ConstantLevel::variable;
                    auto c = ast::as<ast::Call>(io->base);
                    assert(c);
                    if (c->arguments.size() != 1 && c->arguments.size() != 2) {
                        error(io->loc, "expect 1 or 2 arguments but got ", nums(c->arguments.size())).report();
                    }
                    for (auto& arg : c->arguments) {
                        typing_expr(arg, true);
                        io->arguments.push_back(arg);
                    }
                    auto u8_ty = std::make_shared<ast::IntType>(io->loc, 8, ast::Endian::unspec, false);
                    auto arr_ty = std::make_shared<ast::ArrayType>(io->loc, nullptr, io->loc, std::move(u8_ty));
                    io->expr_type = std::move(arr_ty);
                    break;
                }
            }
        }

        void typing_specify_order(ast::SpecifyOrder* b) {
            typing_expr(b->order, false);
            b->base->expr_type = void_type(b->loc);
            b->expr_type = b->base->expr_type;
            b->constant_level = b->order->constant_level;
            ast::tool::Evaluator eval;
            eval.ident_mode = ast::tool::EvalIdentMode::resolve_ident;
            if (auto val = eval.eval(b->order)) {
                // case 1 or 2
                if (val->type() == ast::tool::EResultType::integer) {
                    b->order_value = val->get<ast::tool::EResultType::integer>();
                }
                else if (val->type() == ast::tool::EResultType::boolean) {
                    b->order_value = val->get<ast::tool::EResultType::boolean>() ? 1 : 0;
                }
                else {
                    error(b->order->loc, "expect integer or boolean but got ", ast::tool::eval_result_type_str[int(val->type())]).report();
                }
            }
            b->constant_level = b->order_value ? ast::ConstantLevel::constant : ast::ConstantLevel::immutable_variable;
        }

        void ident_to_type_literal(NodeReplacer base_node, ast::Ident* ident) {
            auto expr = base_node.to_node();
            auto typ = std::make_shared<ast::IdentType>(ident->loc, ast::cast_to<ast::Ident>(expr));
            unwrap_reference_type_from_ident(typ.get());
            auto typ_lit = std::make_shared<ast::TypeLiteral>(ident->loc, std::move(typ), ident->loc);
            typ_lit->expr_type = std::make_shared<ast::MetaType>(ident->loc);
            base_node.replace(std::move(typ_lit));
        }

        void member_access_to_type_literal(NodeReplacer base_node, ast::MemberAccess* selector) {
            auto expr = base_node.to_node();
            auto member = selector->member;
            auto typ = std::make_shared<ast::IdentType>(selector->loc, std::move(member));
            typ->import_ref = ast::cast_to<ast::MemberAccess>(expr);
            unwrap_reference_type_from_ident(typ.get());
            auto typ_lit = std::make_shared<ast::TypeLiteral>(selector->loc, std::move(typ), selector->loc);
            typ_lit->expr_type = std::make_shared<ast::MetaType>(selector->loc);
            base_node.replace(std::move(typ_lit));
        }

        bool try_typing_literal(const std::shared_ptr<ast::Expr>& expr) {
            if (auto lit = ast::as<ast::IntLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::IntLiteralType>(ast::cast_to<ast::IntLiteral>(expr));
                lit->constant_level = ast::ConstantLevel::constant;
                return true;
            }

            if (auto lit = ast::as<ast::BoolLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::BoolType>(lit->loc);
                lit->constant_level = ast::ConstantLevel::constant;
                return true;
            }
            if (auto lit = ast::as<ast::StrLiteral>(expr)) {
                lit->expr_type = std::make_shared<ast::StrLiteralType>(ast::cast_to<ast::StrLiteral>(expr));
                lit->constant_level = ast::ConstantLevel::constant;
                return true;
            }

            if (ast::as<ast::SpecialLiteral>(expr)) {
                // typing already done
                return true;
            }

            if (auto typ = ast::as<ast::TypeLiteral>(expr)) {
                typing_object(typ->type_literal);
                expr->expr_type = std::make_shared<ast::MetaType>(typ->loc);
                expr->constant_level = ast::ConstantLevel::constant;
                return true;
            }

            if (auto ch = ast::as<ast::CharLiteral>(expr)) {
                auto bit = ast::aligned_bit(futils::binary::log2i(ch->code));
                expr->expr_type = std::make_shared<ast::IntType>(ch->loc, bit, ast::Endian::unspec, false);
                expr->constant_level = ast::ConstantLevel::constant;
                return true;
            }

            if (auto regex = ast::as<ast::RegexLiteral>(expr)) {
                expr->expr_type = std::make_shared<ast::RegexLiteralType>(ast::cast_to<ast::RegexLiteral>(expr));
                expr->constant_level = ast::ConstantLevel::constant;
                return true;
            }

            return false;
        }

        bool try_typing_and_maybe_replace_ident(NodeReplacer base_node, std::shared_ptr<ast::Expr>& expr, bool on_define) {
            if (auto ident = ast::as<ast::Ident>(expr)) {
                typing_ident(ast::cast_to<ast::Ident>(expr), on_define);
                if (base_node.place() == ast::NodeType::ident) {
                    return true;  // skip
                }
                if (ident->usage == ast::IdentUsage::reference_type) {
                    ident_to_type_literal(base_node, ident);
                }
                return true;
            }
            return false;
        }

        void typing_or_cond(ast::OrCond* or_cond) {
            std::shared_ptr<ast::Type> ty;
            ast::ConstantLevel level = ast::ConstantLevel::constant;
            for (auto& expr : or_cond->conds) {
                typing_expr(expr);
                if (!ty) {
                    ty = expr->expr_type;
                    level = expr->constant_level;
                }
                else {
                    auto tmp = OrCond_common_type(ty, expr->expr_type);
                    if (!tmp) {
                        report_not_have_common_type(ty, expr->expr_type);
                    }
                    ty = std::move(tmp);
                    level = decide_constant_level(level, expr->constant_level);
                }
            }
            or_cond->expr_type = std::move(ty);
            or_cond->constant_level = level;
        }

        void typing_and_maybe_replace_member_access(ast::MemberAccess* selector, NodeReplacer base_node) {
            typing_member_access(selector);
            if (base_node.place() == ast::NodeType::member_access) {
                return;  // skip
            }
            if (selector->member->usage == ast::IdentUsage::reference_member_type) {
                member_access_to_type_literal(base_node, selector);
            }
        }

        void typing_expr(NodeReplacer base_node, bool on_define = false) {
            auto expr = ast::cast_to<ast::Expr>(base_node.to_node());
            // treat cast as a special case
            // Cast has already been typed in the previous pass
            // but inner expression may not be typed
            if (auto c = ast::as<ast::Cast>(expr)) {
                assert(c->expr_type);
                auto level = ast::ConstantLevel::constant;
                for (auto& args : c->arguments) {
                    typing_expr(args);
                    level = decide_constant_level(level, args->constant_level);
                }
                c->constant_level = level;
            }
            if (auto a = ast::as<ast::Available>(expr)) {
                typing_expr(a->target, false);
                a->constant_level = ast::ConstantLevel::variable;
            }
            if (auto b = ast::as<ast::SpecifyOrder>(expr)) {
                typing_specify_order(b);
            }
            if (auto b = ast::as<ast::ExplicitError>(expr)) {
                typing_expr(b->base->raw_arguments, false);
                b->expr_type = void_type(b->loc);
                b->constant_level = ast::ConstantLevel::variable;
            }
            if (expr->expr_type) {
                typing_object(expr->expr_type);
                return;  // already typed
            }
            if (try_typing_literal(expr)) {
                return;
            }
            else if (auto s = ast::as<ast::IOOperation>(expr)) {
                typing_io_operation(s);
            }
            else if (try_typing_and_maybe_replace_ident(base_node, expr, on_define)) {
                return;
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
                typing_paren(paren);
            }
            else if (auto unary = ast::as<ast::Unary>(expr)) {
                typing_unary(unary);
            }
            else if (auto call = ast::as<ast::Call>(expr)) {
                typing_call(call, base_node);
            }
            else if (auto selector = ast::as<ast::MemberAccess>(expr)) {
                typing_and_maybe_replace_member_access(selector, base_node);
            }
            else if (auto idx = ast::as<ast::Index>(expr)) {
                typing_index(idx);
            }
            else if (ast::as<ast::Range>(expr)) {
                typing_range(ast::cast_to<ast::Range>(expr));
            }
            else if (auto i = ast::as<ast::Import>(expr)) {
                expr->expr_type = i->import_desc->struct_type;
                typing_object(i->import_desc);
                expr->constant_level = ast::ConstantLevel::immutable_variable;
            }
            else if (auto identity = ast::as<ast::Identity>(expr)) {
                typing_expr(identity->expr);
                identity->expr_type = identity->expr->expr_type;
                identity->constant_level = identity->expr->constant_level;
            }
            else if (auto or_cond = ast::as<ast::OrCond>(expr)) {
                typing_or_cond(or_cond);
            }
            else {
                unsupported(expr);
            }
        }

        void unwrap_reference_type_from_ident(ast::IdentType* s) {
            auto map_base_type = [&](ast::Ident* ident) {
                auto member = ast::as<ast::Member>(ident->base.lock());
                if (auto enum_ = ast::as<ast::Enum>(member)) {
                    s->base = enum_->enum_type;
                }
                else if (auto struct_ = ast::as<ast::Format>(member)) {
                    s->base = struct_->body->struct_type;
                }
                else if (auto state_ = ast::as<ast::State>(member)) {
                    s->base = state_->body->struct_type;
                }
            };
            if (s->ident->usage == ast::IdentUsage::reference_type) {
                auto ident = ast::as<ast::Ident>(s->ident->base.lock());
                assert(ident);
                map_base_type(ident);
            }
            if (s->ident->usage == ast::IdentUsage::reference_member_type) {
                assert(s->import_ref);
                auto ident = s->import_ref->base.lock();
                assert(ident);
                map_base_type(ident.get());
            }
        }

        void typing_ident_type(ast::IdentType* s, bool disable_warning = false) {
            // If the object is an identifier, perform identifier typing
            if (s->import_ref) {
                typing_member_access(s->import_ref.get());
            }
            else {
                typing_ident(s->ident, disable_warning);
            }
            if (s->ident->usage == ast::IdentUsage::maybe_type) {
                warn_type_not_found(s->ident);
            }
            if (s->ident->usage != ast::IdentUsage::unknown &&
                s->ident->usage != ast::IdentUsage::reference_type &&
                s->ident->usage != ast::IdentUsage::reference_member_type &&
                s->ident->usage != ast::IdentUsage::maybe_type) {
                auto r = error(s->ident->loc, "expect type name but not");
                if (auto b = s->ident->base.lock()) {
                    if (auto member = ast::as<ast::MemberAccess>(b)) {
                        b = member->base.lock();
                    }
                    if (b) {
                        (void)r.error(b->loc, "identifier ", s->ident->ident, " is defined here");
                    }
                }
                r.report();
            }

            unwrap_reference_type_from_ident(s);

            return;
        }

        void typing_field(ast::Field* field) {
            typing_object(field->field_type);

            if (!field->arguments ||
                field->arguments->collected_arguments.empty() ||
                !field->arguments->arguments.empty() ||
                field->arguments->alignment ||
                field->arguments->sub_byte_length) {
                return;
            }
            auto& args = field->arguments;
            for (auto a : args->collected_arguments) {
                auto arg = a.lock();
                auto conf = ast::tool::extract_config(arg, ast::tool::ExtractMode::assign);
                if (!conf) {
                    typing_expr(arg);
                    if (auto b = ast::as<ast::Binary>(arg); b && ast::is_assign_op(b->op)) {
                        args->assigns.push_back(ast::cast_to<ast::Binary>(std::move(arg)));
                    }
                    else {
                        args->arguments.push_back(std::move(arg));
                    }
                    continue;
                }
                if (conf->name == "input.align") {
                    typing_expr(conf->arguments[0]);
                    args->alignment = std::move(conf->arguments[0]);
                    ast::tool::marking_builtin(ast::as<ast::Binary>(arg)->left);
                    continue;
                }
                if (conf->name == "input.peek") {
                    typing_expr(conf->arguments[0]);
                    args->peek = std::move(conf->arguments[0]);
                    ast::tool::Evaluator eval;
                    eval.ident_mode = ast::tool::EvalIdentMode::resolve_ident;
                    if (auto val = eval.eval(args->peek)) {
                        if (val->type() == ast::tool::EResultType::integer) {
                            args->peek_value = val->get<ast::tool::EResultType::integer>();
                        }
                        else if (val->type() == ast::tool::EResultType::boolean) {
                            args->peek_value = val->get<ast::tool::EResultType::boolean>() ? 1 : 0;
                        }
                        else {
                            error(conf->arguments[0]->loc, "expect integer or boolean but got ", ast::tool::eval_result_type_str[int(val->type())]).report();
                        }
                    }
                    ast::tool::marking_builtin(ast::as<ast::Binary>(arg)->left);
                    continue;
                }
                if (conf->name == "config.type") {
                    typing_expr(conf->arguments[0]);
                    if (!ast::as<ast::TypeLiteral>(conf->arguments[0])) {
                        error(conf->arguments[0]->loc, "expect type literal but not").error(conf->arguments[0]->loc, "type is ", ast::node_type_to_string(conf->arguments[0]->node_type)).report();
                    }
                    args->type_map = ast::cast_to<ast::TypeLiteral>(conf->arguments[0]);
                    ast::tool::marking_builtin(ast::as<ast::Binary>(arg)->left);
                    continue;
                }
                if (conf->name == "input") {
                    auto conf2 = ast::tool::extract_config(conf->arguments[0], ast::tool::ExtractMode::call);
                    // input = expr
                    if (!conf2) {
                        typing_expr(conf->arguments[0]);
                        args->sub_byte_expr = std::move(conf->arguments[0]);
                        continue;
                    }
                    // input.subrange(length_in_bytes,[offset_in_bytes_of_full_input = input.offset])
                    if (conf2->name == "input.subrange") {
                        ast::tool::marking_builtin(ast::as<ast::Call>(conf->arguments[0])->callee);
                        auto loc = conf->arguments[0]->loc;
                        if (conf2->arguments.size() == 1) {
                            // subrange(length) become subrange(length,input.offset)
                            typing_expr(conf2->arguments[0]);
                            args->sub_byte_length = std::move(conf2->arguments[0]);
                        }
                        else if (conf2->arguments.size() == 2) {
                            // subrange(offset,length) become subrange(input.offset+offset,input.offset+offset+length)
                            typing_expr(conf2->arguments[0]);
                            typing_expr(conf2->arguments[1]);
                            args->sub_byte_length = std::move(conf2->arguments[0]);
                            args->sub_byte_begin = std::move(conf2->arguments[1]);
                        }
                        else {
                            error(loc, "expect 1 or 2 arguments but got ", nums(conf2->arguments.size())).report();
                        }
                        continue;
                    }
                }
                auto m = std::make_shared<ast::Metadata>(ast::cast_to<ast::Expr>(arg), std::move(conf->name));
                ast::tool::marking_builtin(ast::as<ast::Binary>(arg)->left);
                m->values = conf->arguments;
                for (auto& v : conf->arguments) {
                    typing_expr(v);
                }
                args->metadata.push_back(std::move(m));
            }

            // analyze fixed mapping type
            if (args->arguments.size() != 0) {
                if (args->arguments.size() != 1) {
                    args->argument_mapping = ast::FieldArgumentMapping::some_candidate;
                }
                auto array_ty = ast::as<ast::ArrayType>(field->field_type);
                for (auto& arg : args->arguments) {
                    auto& typ = arg->expr_type;
                    int_type_fitting(field->field_type, typ);
                    if (comparable_type(field->field_type, typ)) {
                        args->argument_mapping = ast::FieldArgumentMapping(size_t(args->argument_mapping) | size_t(ast::FieldArgumentMapping::direct));
                    }
                    else if (array_ty && (int_type_fitting(array_ty->element_type, typ), comparable_type(array_ty->element_type, typ))) {
                        args->argument_mapping = ast::FieldArgumentMapping(size_t(args->argument_mapping) | size_t(ast::FieldArgumentMapping::repeat));
                    }
                    else if (auto meta = ast::as<ast::TypeLiteral>(arg); meta) {
                        if (args->type_map) {
                            error(meta->loc, "cannot specify type mapping twice").error(args->type_map->loc, "type mapping is specified here").report();
                        }
                        args->type_map = ast::cast_to<ast::TypeLiteral>(arg);
                    }
                    else {
                        auto field_ty = ast::tool::type_to_string(field->field_type);
                        auto arg_ty = ast::tool::type_to_string(typ);
                        if (array_ty) {
                            auto element_ty = ast::tool::type_to_string(array_ty->element_type);
                            error(arg->loc, "cannot decide fixed value argument; expect ", field_ty, " or ", element_ty, " but got ", arg_ty).report();
                        }
                        else {
                            error(arg->loc, "cannot decide fixed value argument; expect ", field_ty, " but got ", arg_ty).report();
                        }
                    }
                }
            }
        }

        void typing_array_type(ast::ArrayType* arr_type) {
            // array like [..]u8 is variable length array, so mark it as variable
            if (ast::is_any_range(arr_type->length)) {
                arr_type->length->constant_level = ast::ConstantLevel::variable;
            }
            // if a->length->constant_level == constant,
            // and eval result has integer type, then a->length_value is set and a->has_const_length is true
            if (arr_type->length && arr_type->length->constant_level == ast::ConstantLevel::constant) {
                ast::tool::Evaluator eval;
                eval.ident_mode = ast::tool::EvalIdentMode::resolve_ident;
                if (auto val = eval.eval_as<ast::tool::EResultType::integer>(arr_type->length)) {
                    arr_type->length_value = val->get<ast::tool::EResultType::integer>();
                }
            }
            // TODO(on-keyday): future, separate phase of typing to disable warning effectively
            if (arr_type->length && arr_type->length->expr_type) {
                for (auto it = warnings.locations.begin(); it != warnings.locations.end();) {
                    if (arr_type->loc.pos.begin <= it->loc.pos.begin && it->loc.pos.end <= arr_type->end_loc.pos.end) {
                        it = warnings.locations.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }
        }

        void typing_union_type(ast::UnionType* u) {
            if (u->common_type) {
                return;
            }
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

        void typing_metadata(ast::Metadata* m) {
            for (auto& v : m->values) {
                typing_expr(v);
            }
        }

        void typing_object(NodeReplacer ty) {
            // Define a lambda function for recursive traversal and typing
            auto recursive_typing = [&](auto&& f, NodeReplacer ty) -> void {
                auto do_traverse = [&] {
                    // Traverse the object's sub components and apply the recursive function
                    ast::traverse(ty.to_node(), [&](auto& sub_ty) -> void {
                        f(f, sub_ty);
                    });
                };
                auto node = ty.to_node();
                if (auto p = ast::as<ast::Program>(node)) {
                    auto tmp = current_global;
                    current_global = p->global_scope;
                    const auto d = futils::helper::defer([&] {
                        current_global = std::move(tmp);
                    });
                    do_traverse();
                    for (auto& elem : p->elements) {
                        if (auto s = ast::as<ast::SpecifyOrder>(elem); s && s->order_type == ast::OrderType::byte) {
                            if (auto l = p->endian.lock()) {
                                warnings
                                    .warning(s->loc, "byte order is specified but endian is already specified. overwritten by after one")
                                    .warning(l->loc, "previous endian is specified here");
                            }
                            p->endian = ast::cast_to<ast::SpecifyOrder>(elem);
                        }
                    }
                    return;
                }
                if (auto s = ast::as<ast::StructType>(node)) {
                    // NOTE(on-keyday): for imported type, we have to typing it prior to use
                    if (auto l = s->base.lock(); l) {
                        if (auto imp = ast::as<ast::Import>(l); imp && !imp->expr_type) {
                            typing_expr(l);
                        }
                    }
                }
                if (auto expr = ast::as<ast::Expr>(node)) {
                    // If the object is an expression, perform expression typing
                    typing_expr(ty);
                    return;
                }
                if (auto s = ast::as<ast::IdentType>(node)) {
                    // If the object is an identifier, perform identifier typing
                    typing_ident_type(s);
                    return;
                }
                if (auto u = ast::as<ast::UnionType>(node)) {
                    typing_union_type(u);
                    return;
                }
                if (auto field = ast::as<ast::Field>(node)) {
                    typing_field(field);
                    return;
                }
                if (auto m = ast::as<ast::Metadata>(node)) {
                    typing_metadata(m);
                    return;
                }
                do_traverse();
                if (auto arr_type = ast::as<ast::ArrayType>(node)) {
                    typing_array_type(arr_type);
                }
                if (auto indent_block = ast::as<ast::IndentBlock>(node)) {
                    for (auto& meta : indent_block->metadata) {
                        if (auto m = meta.lock(); m && m->name == "config.type") {
                            if (m->values.size() && ast::as<ast::TypeLiteral>(m->values[0])) {
                                indent_block->struct_type->type_map = ast::cast_to<ast::TypeLiteral>(m->values[0]);
                            }
                        }
                    }
                }
            };
            recursive_typing(recursive_typing, ty);
        }

       public:
        inline result<void> typing(NodeReplacer ty) {
            try {
                typing_object(ty);
            } catch (LocationError& e) {
                warnings.unique();
                return unexpect(e);
            }
            warnings.unique();
            return {};
        }
    };

    result<void> analyze_type(std::shared_ptr<ast::Program>& node, LocationError* warnings) {
        brgen::LocationError ignore;
        if (!warnings) {
            warnings = &ignore;
        }
        return Typing{*warnings}.typing(node);
    }

}  // namespace brgen::middle
