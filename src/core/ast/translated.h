/*license*/
#pragma once
#include "ast.h"

namespace brgen::ast {
    struct TmpVar : Expr {
        static constexpr ObjectType object_type = ObjectType::tmp_var;
        size_t tmp_index = 0;
        std::shared_ptr<Expr> base;

        TmpVar(std::shared_ptr<Expr>&& c, size_t tmp)
            : Expr(c->loc, ObjectType::tmp_var), tmp_index(tmp), base(std::move(c)) {
            expr_type = base->expr_type;
        }

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("tmp_var", [&](Debug& d) { d.number(tmp_index); });
            });
        }
    };

    struct BlockExpr : Expr {
        node_list calls;
        std::shared_ptr<Expr> expr;

        BlockExpr(std::shared_ptr<Expr>&& a, node_list&& l)
            : Expr(a->loc, ObjectType::block_expr), calls(std::move(l)), expr(std::move(a)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("calls", [&](Debug& d) {
                    d.array([&](auto&& field) {
                        for (auto& c : calls) {
                            field([&](Debug& d) { c->debug(d); });
                        }
                    });
                });
                field("expr", [&](Debug& d) { expr->debug(d); });
            });
        }
    };

}  // namespace brgen::ast
