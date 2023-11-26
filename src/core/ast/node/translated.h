/*license*/
#pragma once
#include "base.h"

namespace brgen::ast {
    struct TmpVar : Expr {
        define_node_type(NodeType::tmp_var);
        size_t tmp_var = 0;
        std::shared_ptr<Expr> base;

        TmpVar(std::shared_ptr<Expr>&& c, size_t tmp)
            : Expr(c->loc, NodeType::tmp_var), tmp_var(tmp), base(std::move(c)) {
            expr_type = base->expr_type;
        }

        TmpVar()
            : Expr({}, NodeType::tmp_var) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(tmp_var);
        }
    };

    struct BlockExpr : Expr {
        define_node_type(NodeType::block_expr);
        node_list calls;
        std::shared_ptr<Expr> expr;

        BlockExpr(std::shared_ptr<Expr>&& a, node_list&& l)
            : Expr(a->loc, NodeType::block_expr), calls(std::move(l)), expr(std::move(a)) {}

        BlockExpr()
            : Expr({}, NodeType::block_expr) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(calls);
            sdebugf(expr);
        }
    };

    struct Assert : Stmt {
        define_node_type(NodeType::assert);
        std::shared_ptr<Binary> cond;

        Assert(std::shared_ptr<Binary>&& a)
            : Stmt(a->loc, NodeType::assert), cond(std::move(a)) {}

        Assert()
            : Stmt({}, NodeType::assert) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(cond);
        }
    };

    struct ImplicitYield : Stmt {
        define_node_type(NodeType::implicit_yield);
        std::shared_ptr<Expr> expr;

        ImplicitYield(std::shared_ptr<Expr>&& a)
            : Stmt(a->loc, NodeType::implicit_yield), expr(std::move(a)) {}

        ImplicitYield()
            : Stmt({}, NodeType::implicit_yield) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(expr);
        }
    };

    struct Import : Expr {
        define_node_type(NodeType::import_);
        std::string path;
        std::shared_ptr<Call> base;
        std::shared_ptr<Program> import_desc;

        Import(std::shared_ptr<Call>&& c, std::shared_ptr<Program>&& a, std::string&& p)
            : Expr(a->loc, NodeType::import_), path(std::move(p)), base(std::move(c)), import_desc(std::move(a)) {
            expr_type = import_desc->struct_type;
        }

        Import()
            : Expr({}, NodeType::import_) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(path);
            sdebugf_omit(base);
            sdebugf(import_desc);
        }
    };

    struct Cast : Expr {
        define_node_type(NodeType::cast);
        std::shared_ptr<Call> base;
        std::shared_ptr<Expr> expr;

        Cast(std::shared_ptr<Call>&& c, std::shared_ptr<Type>&& type, std::shared_ptr<Expr>&& a)
            : Expr(a->loc, NodeType::cast), base(std::move(c)), expr(std::move(a)) {
            expr_type = std::move(type);
        }

        Cast()
            : Expr({}, NodeType::cast) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf_omit(base);
            sdebugf(expr);
        }
    };

    // Available represents available(ident) expression
    // ident should be field name
    struct Available : Expr {
        define_node_type(NodeType::available);
        std::shared_ptr<Call> base;
        std::shared_ptr<Ident> target;

        Available(std::shared_ptr<Ident>&& a, std::shared_ptr<Call>&& c)
            : Expr(a->loc, NodeType::available), base(std::move(c)), target(std::move(a)) {}

        Available()
            : Expr({}, NodeType::available) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf_omit(base);
            sdebugf(target);
        }
    };

}  // namespace brgen::ast
