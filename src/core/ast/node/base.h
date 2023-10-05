/*license*/
#pragma once
#include "node_type.h"
#include <core/common/debug.h>
#include <core/lexer/token.h>
#include <list>

namespace brgen::ast {
    constexpr void as_json(NodeType type, auto&& buf) {
        buf.value(node_type_to_string(type));
    }

    // abstract

    struct Scope;
    using scope_ptr = std::shared_ptr<Scope>;
    struct Node {
        const NodeType node_type;
        lexer::Loc loc;

        void dump(auto&& field) {
            sdebugf(node_type);
            sdebugf(loc);
        }

       protected:
        constexpr Node(lexer::Loc l, NodeType t)
            : loc(l), node_type(t) {}
    };

#define define_node_type(type) \
    static constexpr NodeType node_type_tag = type

    struct Type : Node {
        define_node_type(NodeType::type);

       protected:
        constexpr Type(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Expr : Node {
        define_node_type(NodeType::expr);
        std::shared_ptr<Type> expr_type;

        void dump(auto&& field) {
            Node::dump(field);
            sdebugf(expr_type);
        }

       protected:
        constexpr Expr(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Stmt : Node {
        define_node_type(NodeType::stmt);

       protected:
        constexpr Stmt(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Member : Stmt {
        define_node_type(NodeType::member);

       protected:
        constexpr Member(lexer::Loc l, NodeType t)
            : Stmt(l, t) {}
    };

    struct Literal : Expr {
        define_node_type(NodeType::literal);

       protected:
        constexpr Literal(lexer::Loc l, NodeType t)
            : Expr(l, t) {}
    };

    using node_list = std::list<std::shared_ptr<Node>>;

    struct IndentScope : Stmt {
        define_node_type(NodeType::indent_scope);
        node_list elements;
        scope_ptr scope;

        IndentScope(lexer::Loc l)
            : Stmt(l, NodeType::indent_scope) {}

        // for decode
        IndentScope()
            : Stmt({}, NodeType::indent_scope) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            sdebugf(elements);
            sdebugf(scope);
        }
    };

    struct StructType;

    struct Program : Node {
        define_node_type(NodeType::program);
        std::shared_ptr<StructType> struct_type;
        node_list elements;
        scope_ptr global_scope;

        void dump(auto&& field) {
            Node::dump(field);
            sdebugf(struct_type);
            sdebugf(elements);
            sdebugf(global_scope);
        }

        Program()
            : Node(lexer::Loc{}, NodeType::program) {}
    };

}  // namespace brgen::ast
