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

        virtual ~Node() {}

        virtual void as_json(Debug& buf) {
            auto field = buf.object();
            dump(field);
        }

        void dump(auto&& field) {
            field(sdebugf(node_type));
            field(sdebugf(loc));
        }

       protected:
        constexpr Node(lexer::Loc l, NodeType t)
            : loc(l), node_type(t) {}
    };

    template <class B>
    struct FieldWrapper {
        B& base;

        constexpr auto operator()(std::string_view key, auto&& value) const {
            using P = utils::helper::template_of_t<std::decay_t<decltype(value)>>;
            if constexpr (std::is_same_v<decltype(value), scope_ptr&>) {
                if (key == "global_scope") {
                    base(key, value);
                }
            }
            else if constexpr (P::value) {
                if constexpr (utils::helper::is_template_instance_of<std::decay_t<decltype(value)>, std::weak_ptr>) {
                    // ignore at here
                }
                else {
                    using P2 = utils::helper::template_of_t<typename P::template param_at<0>>;
                    if constexpr (P2::value) {
                        if constexpr (utils::helper::is_template_instance_of<typename P2::instance, std::weak_ptr>) {
                            // ignore at here
                        }
                        else {
                            base(key, value);
                        }
                    }
                }
            }
            else {
                base(key, value);
            }
        }
    };

#define define_node_type(type)                               \
    virtual void as_json(Debug& buf) override {              \
        auto field = buf.object();                           \
        auto wrapper = FieldWrapper<decltype(field)>{field}; \
        dump(wrapper);                                       \
    }                                                        \
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
            field(sdebugf(expr_type));
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
            field(sdebugf(elements));
            field(sdebugf(scope));
        }
    };

    struct Program : Node {
        define_node_type(NodeType::program);
        node_list elements;
        scope_ptr global_scope;

        void dump(auto&& field) {
            Node::dump(field);
            field(sdebugf(elements));
            field(sdebugf(global_scope));
        }

        Program()
            : Node(lexer::Loc{}, NodeType::program) {}
    };

}  // namespace brgen::ast
