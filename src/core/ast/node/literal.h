/*license*/
#pragma once
#include "base.h"
#include <optional>
#include <number/prefix.h>

namespace brgen::ast {
    // literals
    struct IntLiteral : Literal {
        define_node_type(NodeType::int_literal);
        std::string value;

        template <class T>
        std::optional<T> parse_as() const {
            T t = 0;
            if (!utils::number::prefix_integer(value, t)) {
                return std::nullopt;
            }
            return t;
        }

        void dump(auto&& field) {
            Literal::dump(field);
            sdebugf(value);
        }

        IntLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, NodeType::int_literal), value(std::move(t)) {}

        // for decode
        IntLiteral()
            : Literal({}, NodeType::int_literal) {}
    };

    struct StrLiteral : Literal {
        define_node_type(NodeType::str_literal);
        std::string value;

        StrLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, NodeType::str_literal), value(std::move(t)) {}

        // for decode
        StrLiteral()
            : Literal({}, NodeType::str_literal) {}

        void dump(auto&& field) {
            Literal::dump(field);
            sdebugf(value);
        }
    };

    struct BoolLiteral : Literal {
        define_node_type(NodeType::bool_literal);
        bool value = false;

        void dump(auto&& field) {
            Literal::dump(field);
            sdebugf(value);
        }

        BoolLiteral(lexer::Loc l, bool t)
            : Literal(l, NodeType::bool_literal), value(t) {}

        // for decode
        constexpr BoolLiteral()
            : Literal({}, NodeType::bool_literal) {}
    };

    struct Input : Literal {
        define_node_type(NodeType::input);

        Input(lexer::Loc l, const std::shared_ptr<Type>& d)
            : Literal(l, NodeType::input) {
            expr_type = d;
        }

        // for decode
        constexpr Input()
            : Literal({}, NodeType::input) {}
    };

    struct Output : Literal {
        define_node_type(NodeType::output);

        Output(lexer::Loc l, const std::shared_ptr<Type>& d)
            : Literal(l, NodeType::output) {
            expr_type = d;
        }

        // for decode
        constexpr Output()
            : Literal({}, NodeType::output) {}
    };

    struct Config : Literal {
        define_node_type(NodeType::config);

        Config(lexer::Loc l, const std::shared_ptr<Type>& d)
            : Literal(l, NodeType::config) {
            expr_type = d;
        }

        constexpr Config()
            : Literal({}, NodeType::config) {}
    };

}  // namespace brgen::ast
