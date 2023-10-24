/*license*/
#pragma once
#include "node_type.h"
#include <core/common/debug.h>
#include <core/lexer/token.h>
#include <list>
#include <vector>

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
        bool is_explicit = false;  // for language server annotation

        void dump(auto&& field) {
            Node::dump(field);
            sdebugf(is_explicit);
        }

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
        std::weak_ptr<Member> belong;

        void dump(auto&& field) {
            Stmt::dump(field);
            sdebugf(belong);
        }

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

    struct Comment : Node {
        define_node_type(NodeType::comment);
        std::string comment;

        void dump(auto&& field) {
            Node::dump(field);
            sdebugf(comment);
        }

        Comment(lexer::Loc l, const std::string& c)
            : Node(l, NodeType::comment), comment(std::move(c)) {}

        Comment()
            : Node({}, NodeType::comment) {}
    };

    struct CommentGroup : Node {
        define_node_type(NodeType::comment_group);
        std::vector<std::shared_ptr<Comment>> comments;

        void dump(auto&& field) {
            Node::dump(field);
            sdebugf(comments);
        }

        CommentGroup(lexer::Loc l, std::vector<std::shared_ptr<Comment>>&& c)
            : Node(l, NodeType::comment_group), comments(std::move(c)) {}
        CommentGroup()
            : Node({}, NodeType::comment_group) {}
    };

    using node_list = std::list<std::shared_ptr<Node>>;

    struct IndentBlock : Stmt {
        define_node_type(NodeType::indent_block);
        node_list elements;
        scope_ptr scope;

        IndentBlock(lexer::Loc l)
            : Stmt(l, NodeType::indent_block) {}

        // for decode
        IndentBlock()
            : Stmt({}, NodeType::indent_block) {}

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
