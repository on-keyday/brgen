/*license*/
#pragma once
#include "node_type.h"
#include <core/common/debug.h>
#include <core/lexer/token.h>

namespace brgen::ast {
    // abstract
    struct Node {
        const NodeType type;
        lexer::Loc loc;

        virtual ~Node() {}

        virtual void as_json(Debug& buf) const {
            auto field = buf.object();
            basic_info(field);
        }

        void basic_info(auto&& field) const {
            field("node_type", node_type_to_string(type));
            field(sdebugf(loc));
        }

       protected:
        constexpr Node(lexer::Loc l, NodeType t)
            : loc(l), type(t) {}
    };

    struct Type : Node {
        static constexpr NodeType node_type = NodeType::type;

       protected:
        constexpr Type(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Expr : Node {
        static constexpr NodeType node_type = NodeType::expr;
        std::shared_ptr<Type> expr_type;

       protected:
        constexpr Expr(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Stmt : Node {
        static constexpr NodeType node_type = NodeType::stmt;

       protected:
        constexpr Stmt(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Literal : Expr {
       protected:
        constexpr Literal(lexer::Loc l, NodeType t)
            : Expr(l, t) {}
    };
}  // namespace brgen::ast
