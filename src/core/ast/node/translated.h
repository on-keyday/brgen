/*license*/
#pragma once
#include <memory>
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
        std::vector<std::shared_ptr<Expr>> arguments;

        Cast(std::shared_ptr<Call>&& c, std::shared_ptr<Type>&& type, const std::vector<std::shared_ptr<Expr>>& a)
            : Expr(c->loc, NodeType::cast), base(std::move(c)), arguments(a) {
            expr_type = std::move(type);
        }

        Cast()
            : Expr({}, NodeType::cast) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf_omit(base);
            sdebugf(arguments);
        }
    };

    // Available represents available(ident) expression
    // ident should be field name
    struct Available : Expr {
        define_node_type(NodeType::available);
        std::shared_ptr<Call> base;
        std::shared_ptr<Expr> target;
        std::shared_ptr<Type> expected_type;

        Available(std::shared_ptr<Expr>&& a, std::shared_ptr<Call>&& c)
            : Expr(a->loc, NodeType::available), base(std::move(c)), target(std::move(a)) {}

        Available()
            : Expr({}, NodeType::available) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf_omit(base);
            sdebugf(target);
            sdebugf(expected_type);
        }
    };

    // SizeOf represents sizeof(ident) expression
    // ident maybe type literal or expression
    struct SizeOf : Expr {
        define_node_type(NodeType::sizeof_);
        std::shared_ptr<Call> base;
        std::shared_ptr<Expr> target;

        SizeOf(std::shared_ptr<Expr>&& a, std::shared_ptr<Call>&& c)
            : Expr(a->loc, NodeType::sizeof_), base(std::move(c)), target(std::move(a)) {}

        SizeOf()
            : Expr({}, NodeType::sizeof_) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf_omit(base);
            sdebugf(target);
        }
    };

    struct SpecifyOrder : Expr {
        define_node_type(NodeType::specify_order);
        OrderType order_type = OrderType::byte;
        std::shared_ptr<Binary> base;
        // 0(or false) is big/msb, 1(or true) is little/lsb,2 is native, otherwise is unspecified(default to big/msb) or generator dependent
        std::shared_ptr<Expr> order;
        std::optional<size_t> order_value;

        SpecifyOrder(std::shared_ptr<Binary>&& a, std::shared_ptr<Expr>&& b, OrderType order_type)
            : Expr(a->loc, NodeType::specify_order), base(std::move(a)), order(std::move(b)), order_type(order_type) {}

        SpecifyOrder()
            : Expr({}, NodeType::specify_order) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(base);
            sdebugf(order_type);
            sdebugf(order);
            sdebugf(order_value);
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

    struct Metadata : Stmt {
        define_node_type(NodeType::metadata);
        std::shared_ptr<Expr> base;
        std::string name;
        std::vector<std::shared_ptr<Expr>> values;

        Metadata(std::shared_ptr<Expr>&& a, std::string&& b)
            : Stmt(a->loc, NodeType::metadata), base(std::move(a)), name(std::move(b)) {}

        Metadata()
            : Stmt({}, NodeType::metadata) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(base);
            sdebugf(name);
            sdebugf(values);
        }
    };

}  // namespace brgen::ast
