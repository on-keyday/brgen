/*license*/
#pragma once
#include "node_type.h"
#include <core/common/debug.h>
#include <core/lexer/token.h>
#include <vector>
#include <optional>
#include "ast_enum.h"

namespace brgen::ast {
    constexpr void as_json(NodeType type, auto&& buf) {
        buf.value(node_type_to_string(type));
    }

    // abstract

    struct Scope;
    using scope_ptr = std::shared_ptr<Scope>;

#define define_node_description(desc) \
    static constexpr const char* node_type_description = desc
    struct Node {
        define_node_description(
            "abstract node. all node inherit this class.\n"
            "this class has node type and location.");
        const NodeType node_type;
        lexer::Loc loc;

        void dump(auto&& field_) {
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
        define_node_description(
            "abstract type class\n"
            " this class has type attributes.\n");
        bool is_explicit = false;  // for language server annotation
        // type is integer or float, set of integer or float, or fixed length integer or float array.
        // not complex type array, not dynamic array, not include recursive struct
        bool non_dynamic_allocation = false;
        // type is interpretable as single integer type
        // bool single_int_type = false;
        // bit alignment of type
        BitAlignment bit_alignment = BitAlignment::not_target;
        // bit size of type. if dynamic length or not decidable, nullopt.
        std::optional<size_t> bit_size = std::nullopt;

        void dump(auto&& field_) {
            Node::dump(field_);
            sdebugf(is_explicit);
            sdebugf(non_dynamic_allocation);
            sdebugf(bit_alignment);
            sdebugf(bit_size);
        }

       protected:
        constexpr Type(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Expr : Node {
        define_node_type(NodeType::expr);
        define_node_description(
            "abstract expression class\n"
            " this class has type information and constant level.");

        std::shared_ptr<Type> expr_type;
        ConstantLevel constant_level = ConstantLevel::unknown;

        void dump(auto&& field_) {
            Node::dump(field_);
            sdebugf(expr_type);
            sdebugf(constant_level);
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

    struct Ident;
    struct StructType;

    struct Member : Stmt {
        define_node_type(NodeType::member);
        std::weak_ptr<Member> belong;
        std::weak_ptr<StructType> belong_struct;
        std::shared_ptr<Ident> ident;

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(belong);
            sdebugf(belong_struct);
            sdebugf(ident);
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

        void dump(auto&& field_) {
            Node::dump(field_);
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

        void dump(auto&& field_) {
            Node::dump(field_);
            sdebugf(comments);
        }

        CommentGroup(lexer::Loc l, std::vector<std::shared_ptr<Comment>>&& c)
            : Node(l, NodeType::comment_group), comments(std::move(c)) {}
        CommentGroup()
            : Node({}, NodeType::comment_group) {}
    };

    using node_list = std::vector<std::shared_ptr<Node>>;
    struct StructType;
    struct ScopedStatement : Stmt {
        define_node_type(NodeType::scoped_statement);
        std::shared_ptr<Node> statement;
        scope_ptr scope;
        std::shared_ptr<StructType> struct_type;

        ScopedStatement(lexer::Loc l)
            : Stmt(l, NodeType::scoped_statement) {}

        ScopedStatement()
            : Stmt({}, NodeType::scoped_statement) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(struct_type);
            sdebugf(statement);
            sdebugf(scope);
        }
    };

    struct Metadata;

    struct IndentBlock : Stmt {
        define_node_type(NodeType::indent_block);
        node_list elements;
        scope_ptr scope;
        std::shared_ptr<StructType> struct_type;
        /*
        mapped type by config.type
        for example
            ```
            format A:
                config.type = u64
                prefix :u2
                match prefix:
                    0 => value :u8
                    1 => value :u16
                    2 => value :u32
                    3 => value :u64
            ```
        */
        std::shared_ptr<TypeLiteral> type_map;
        std::vector<std::weak_ptr<Metadata>> metadata;

        BlockTrait block_traits = BlockTrait::none;

        IndentBlock(lexer::Loc l)
            : Stmt(l, NodeType::indent_block) {}

        // for decode
        IndentBlock()
            : Stmt({}, NodeType::indent_block) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(struct_type);
            sdebugf(elements);
            sdebugf(scope);
            sdebugf_omit(metadata);
            sdebugf(type_map);
            sdebugf(block_traits);
        }
    };

    struct SpecifyOrder;

    struct Program : Node {
        define_node_type(NodeType::program);
        std::shared_ptr<StructType> struct_type;
        node_list elements;
        scope_ptr global_scope;
        std::vector<std::weak_ptr<Metadata>> metadata;
        std::weak_ptr<SpecifyOrder> endian;

        void dump(auto&& field_) {
            Node::dump(field_);
            sdebugf(struct_type);
            sdebugf(elements);
            sdebugf(global_scope);
            sdebugf_omit(metadata);
            sdebugf_omit(endian);
        }

        Program()
            : Node(lexer::Loc{}, NodeType::program) {}
    };

}  // namespace brgen::ast
