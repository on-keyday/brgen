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

       private:
        bool unique_ident(const std::shared_ptr<Expr>& expr, std::optional<std::shared_ptr<Ident>>& ident) {
            if (auto e = as<Binary>(expr)) {
                return unique_ident(e->left, ident) && unique_ident(e->right, ident);
            }
            else if (auto e = as<Ident>(expr)) {
                if (ident) {
                    return e->ident == (*ident)->ident &&
                           is_field_of(e) == is_field_of(*ident);
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
            return false;
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
                switch (e->op) {
                    case BinaryOp::add: {
                                        }
                }
            }
            else if (auto e = as<Ident>(expr)) {
                has_ident = true;
                return expr;
            }
            else if (auto lit = as<IntLiteral>(expr)) {
            }
            else if (auto e = as<Unary>(expr)) {
                return resolve_liner_equation(e->expr, has_ident);
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
            resolved = *ident;
            bool has_ident = false;
            resolve_liner_equation(expr, has_ident);
        }
    };

}  // namespace brgen::ast::tool
