/*license*/
#pragma once
#include "base.h"
#include "expr.h"
#include "type.h"

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

    struct Assert : Stmt {
        define_node_type(NodeType::assert);
        std::shared_ptr<Binary> cond;
        bool is_io_related = false;

        Assert(std::shared_ptr<Binary>&& a)
            : Stmt(a->loc, NodeType::assert), cond(std::move(a)) {}

        Assert()
            : Stmt({}, NodeType::assert) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(cond);
            sdebugf(is_io_related);
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
            : Expr(c->loc, NodeType::import_), path(std::move(p)), base(std::move(c)), import_desc(std::move(a)) {
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
            : Expr(c->loc, NodeType::cast), base(std::move(c)), expr(std::move(a)) {
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
        std::shared_ptr<Expr> target;

        Available(std::shared_ptr<Expr>&& a, std::shared_ptr<Call>&& c)
            : Expr(a->loc, NodeType::available), base(std::move(c)), target(std::move(a)) {}

        Available()
            : Expr({}, NodeType::available) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf_omit(base);
            sdebugf(target);
        }
    };

    struct SpecifyOrder : Expr {
        define_node_type(NodeType::specify_order);
        std::shared_ptr<Binary> base;
        // 0(or false) is big, 1(or true) is little,2 is native, otherwise is unspecified(default to big) or generator dependent
        std::shared_ptr<Expr> endian;
        std::optional<size_t> endian_value;

        SpecifyOrder(std::shared_ptr<Binary>&& a, std::shared_ptr<Expr>&& b)
            : Expr(a->loc, NodeType::specify_order), base(std::move(a)), endian(std::move(b)) {}

        SpecifyOrder()
            : Expr({}, NodeType::specify_order) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(base);
            sdebugf(endian);
            sdebugf(endian_value);
        }
    };

    struct ExplicitError : Expr {
        define_node_type(NodeType::explicit_error);
        std::shared_ptr<ast::Call> base;
        std::shared_ptr<ast::StrLiteral> message;

        ExplicitError(std::shared_ptr<ast::Call>&& a, std::shared_ptr<ast::StrLiteral>&& b)
            : Expr(a->loc, NodeType::explicit_error), base(std::move(a)), message(std::move(b)) {}

        ExplicitError()
            : Expr({}, NodeType::explicit_error) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(base);
            sdebugf(message);
        }
    };

    // TODO(on-keyday): remove this in the future with the 'builtin' system
    struct IOOperation : Expr {
        define_node_type(NodeType::io_operation);
        define_node_description(R"(IOOperation represents input/output/config operation.)");
        IOMethod method = IOMethod::unspec;
        std::shared_ptr<Expr> base;
        std::vector<std::shared_ptr<Expr>> arguments;

        IOOperation(std::shared_ptr<Expr>&& a, IOMethod m)
            : Expr(a->loc, NodeType::io_operation), base(std::move(a)), method(m) {}

        IOOperation()
            : Expr({}, NodeType::io_operation) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(base);
            sdebugf(method);
            sdebugf(arguments);
        }
    };

}  // namespace brgen::ast
