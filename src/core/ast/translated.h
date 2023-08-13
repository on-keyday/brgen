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
            auto field = buf.object();
            field("tmp_var", tmp_index);
        }
    };

    struct BlockExpr : Expr {
        node_list calls;
        std::shared_ptr<Expr> expr;

        BlockExpr(std::shared_ptr<Expr>&& a, node_list&& l)
            : Expr(a->loc, ObjectType::block_expr), calls(std::move(l)), expr(std::move(a)) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(calls));
            field(sdebugf(expr));
        }
    };

}  // namespace brgen::ast
