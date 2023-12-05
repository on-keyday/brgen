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

    enum class BitAlignment {
        byte_aligned,
        bit_1,
        bit_2,
        bit_3,
        bit_4,
        bit_5,
        bit_6,
        bit_7,
        not_target,
        not_decidable,
    };

    constexpr const char* bit_alignment_str[] = {
        "byte_aligned",
        "bit_1",
        "bit_2",
        "bit_3",
        "bit_4",
        "bit_5",
        "bit_6",
        "bit_7",
        "not_target",
        "not_decidable",
        nullptr,
    };

    constexpr size_t bit_alignment_count = std::size(bit_alignment_str) - 1;

    constexpr void as_json(BitAlignment alignment, auto&& buf) {
        buf.value(bit_alignment_str[int(alignment)]);
    }

    constexpr std::optional<BitAlignment> bit_alignment(std::string_view str) {
        for (int i = 0; bit_alignment_str[i]; i++) {
            if (bit_alignment_str[i] == str) {
                return BitAlignment(i);
            }
        }
        return std::nullopt;
    }

    struct Type : Node {
        define_node_type(NodeType::type);
        bool is_explicit = false;  // for language server annotation
        // type is integer, set of integer, or fixed length integer array.
        // not complex type array, not dynamic array, not include recursive struct
        bool is_int_set = false;
        // type is interpretable as single integer type
        // bool single_int_type = false;
        // bit alignment of type
        BitAlignment bit_alignment = BitAlignment::not_target;
        // bit size of type. if dynamic length or not decidable, 0.
        // TODO(on-keyday): actual zero size type is what?
        size_t bit_size = 0;

        void dump(auto&& field_) {
            Node::dump(field_);
            sdebugf(is_explicit);
            sdebugf(is_int_set);
            sdebugf(bit_alignment);
            sdebugf(bit_size);
        }

       protected:
        constexpr Type(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    enum class ConstantLevel {
        unknown,         // not determined
        constant,        // decided at compile time
        const_variable,  // decided at runtime, but not changed
        variable,        // changed at runtime
    };

    constexpr const char* constant_level_str[] = {
        "unknown",
        "constant",
        "const_variable",
        "variable",
        nullptr,
    };

    constexpr size_t constant_level_count = 4;

    constexpr void as_json(ConstantLevel level, auto&& buf) {
        buf.value(constant_level_str[int(level)]);
    }

    constexpr std::optional<ConstantLevel> constant_level(std::string_view str) {
        for (int i = 0; constant_level_str[i]; i++) {
            if (constant_level_str[i] == str) {
                return ConstantLevel(i);
            }
        }
        return std::nullopt;
    }

    struct Expr : Node {
        define_node_type(NodeType::expr);
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

    using node_list = std::list<std::shared_ptr<Node>>;
    struct StructType;
    struct IndentBlock : Stmt {
        define_node_type(NodeType::indent_block);
        node_list elements;
        scope_ptr scope;
        std::shared_ptr<StructType> struct_type;

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
        }
    };

    struct Program : Node {
        define_node_type(NodeType::program);
        std::shared_ptr<StructType> struct_type;
        node_list elements;
        scope_ptr global_scope;

        void dump(auto&& field_) {
            Node::dump(field_);
            sdebugf(struct_type);
            sdebugf(elements);
            sdebugf(global_scope);
        }

        Program()
            : Node(lexer::Loc{}, NodeType::program) {}
    };

}  // namespace brgen::ast
