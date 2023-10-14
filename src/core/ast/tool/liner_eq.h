/*license*/
#pragma once
#include "type.h"
#include <set>

namespace brgen::ast::tool {

    //  x = a * y + b
    //  x - b = a * y
    //  (x - b) / a = y
    //  y = (x - b) / a

    struct LinerResolver {
        std::shared_ptr<Expr> resolved;
        std::shared_ptr<Ident> about;
        std::shared_ptr<TmpVar> x;

       private:
        bool unique_ident(const std::shared_ptr<Expr>& expr, std::optional<std::shared_ptr<Ident>>& ident) {
            if (auto e = as<Binary>(expr)) {
                return unique_ident(e->left, ident) && unique_ident(e->right, ident);
            }
            else if (auto e = as<Ident>(expr)) {
                if (ident) {
                    return e->ident == (*ident)->ident &&
                           belong_format(e) == belong_format(*ident);
                }
                else {
                    ident = cast_to<Ident>(expr);
                    return true;
                }
            }
            else if (auto e = as<Unary>(expr)) {
                return unique_ident(e->expr, ident);
            }
            else if (auto p = as<Paren>(expr)) {
                return unique_ident(p->expr, ident);
            }
            else if (auto i = as<Call>(expr)) {
                return false;  // not supported yet
            }
            else if (auto i = as<MemberAccess>(expr)) {
                return false;  // not supported yet
            }
            return true;
        }

        // y = 2x + (x<<2)
        // y = 2x + (x * 4)
        // y = 2x + 4x
        // y = 6x
        // x = y / 6
        std::shared_ptr<Expr> resolve_liner_equation(const std::shared_ptr<Expr>& expr, bool& has_ident) {
            if (auto e = as<Binary>(expr)) {
                bool left_has_ident = false, right_has_ident = false;
                auto l = resolve_liner_equation(e->left, left_has_ident);
                auto r = resolve_liner_equation(e->right, right_has_ident);
                if (!l || !r) {
                    return nullptr;
                }
                if (left_has_ident && right_has_ident) {
                    return nullptr;  // not support yet
                }
                switch (e->op) {
                    case BinaryOp::add: {
                        if (left_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::sub);
                            b->right = std::move(r);
                            return b;
                        }
                        else if (right_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(r), BinaryOp::sub);
                            b->right = std::move(l);
                            return b;
                        }
                        else {
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::add);
                            b->right = std::move(r);
                            return b;
                        }
                    }
                    case BinaryOp::sub: {
                        if (left_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::add);
                            b->right = std::move(r);
                            return b;
                        }
                        else if (right_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(r), BinaryOp::add);
                            b->right = std::move(l);
                            return b;
                        }
                        else {
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::sub);
                            b->right = std::move(r);
                            return b;
                        }
                    }
                    case BinaryOp::mul: {
                        if (left_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::div);
                            b->right = std::move(r);
                            return b;
                        }
                        else if (right_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(r), BinaryOp::div);
                            b->right = std::move(l);
                            return b;
                        }
                        else {
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::mul);
                            b->right = std::move(r);
                            return b;
                        }
                    }
                    case BinaryOp::div: {
                        if (left_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::mul);
                            b->right = std::move(r);
                            return b;
                        }
                        else if (right_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(r), BinaryOp::mul);
                            b->right = std::move(l);
                            return b;
                        }
                        else {
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::div);
                            b->right = std::move(r);
                            return b;
                        }
                    }
                    case BinaryOp::left_logical_shift: {
                        if (left_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::right_logical_shift);
                            b->right = std::move(r);
                            return b;
                        }
                        else if (right_has_ident) {
                            return nullptr;
                        }
                        else {
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::left_logical_shift);
                            b->right = std::move(r);
                            return b;
                        }
                    }
                    case BinaryOp::right_logical_shift: {
                        if (left_has_ident) {
                            has_ident = true;
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::left_logical_shift);
                            b->right = std::move(r);
                            return b;
                        }
                        else if (right_has_ident) {
                            return nullptr;
                        }
                        else {
                            auto b = std::make_shared<Binary>(e->loc, std::move(l), BinaryOp::right_logical_shift);
                            b->right = std::move(r);
                            return b;
                        }
                    }
                    default: {
                        return nullptr;
                    }
                }
            }
            else if (auto e = as<Ident>(expr)) {
                if (expr != about) {
                    return nullptr;
                }
                has_ident = true;
                auto rep = expr;
                x = std::make_shared<TmpVar>(std::move(rep), 0);
                return x;
            }
            else if (as<IntLiteral>(expr)) {
                return expr;
            }
            else if (auto e = as<Unary>(expr)) {
                auto t = resolve_liner_equation(e->expr, has_ident);
                if (!t) {
                    return nullptr;
                }
                auto u = std::make_shared<Unary>(e->loc, e->op);
                u->expr = std::move(t);
                return u;
            }
            else if (auto p = as<Paren>(expr)) {
                return resolve_liner_equation(p->expr, has_ident);
            }
            // do nothing
            return nullptr;
        }

       public:
        bool resolve(const std::shared_ptr<Expr>& expr) {
            std::optional<std::shared_ptr<Ident>> ident;
            if (!unique_ident(expr, ident)) {
                return false;
            }
            about = std::move(*ident);
            bool has_ident = false;
            auto r = resolve_liner_equation(expr, has_ident);
            if (!r) {
                return false;
            }
            resolved = std::move(r);
            return true;
        }
    };

}  // namespace brgen::ast::tool
