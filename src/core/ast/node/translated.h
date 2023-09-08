/*license*/
#pragma once
#include "base.h"
#include "ast.h"

namespace brgen::ast {
    struct TmpVar : Expr {
        define_node_type(NodeType::tmp_var);
        size_t tmp_index = 0;
        std::shared_ptr<Expr> base;

        TmpVar(std::shared_ptr<Expr>&& c, size_t tmp)
            : Expr(c->loc, NodeType::tmp_var), tmp_index(tmp), base(std::move(c)) {
            expr_type = base->expr_type;
        }

        void dump(auto&& field) {
            Expr::dump(field);
            field("tmp_var", tmp_index);
        }
    };

    struct BlockExpr : Expr {
        define_node_type(NodeType::block_expr);
        node_list calls;
        std::shared_ptr<Expr> expr;

        BlockExpr(std::shared_ptr<Expr>&& a, node_list&& l)
            : Expr(a->loc, NodeType::block_expr), calls(std::move(l)), expr(std::move(a)) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(calls));
            field(sdebugf(expr));
        }
    };

    struct Assert : Stmt {
        define_node_type(NodeType::assert);
        std::shared_ptr<Binary> cond;

        Assert(std::shared_ptr<Binary>&& a)
            : Stmt(a->loc, NodeType::assert), cond(std::move(a)) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(cond));
        }
    };

    struct ImplicitReturn : Stmt {
        define_node_type(NodeType::implicit_return);
        std::shared_ptr<Expr> expr;

        ImplicitReturn(std::shared_ptr<Expr>&& a)
            : Stmt(a->loc, NodeType::implicit_return), expr(std::move(a)) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(expr));
        }
    };

}  // namespace brgen::ast
