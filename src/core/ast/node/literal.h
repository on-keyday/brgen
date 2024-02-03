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
            if (!futils::number::prefix_integer(value, t)) {
                return std::nullopt;
            }
            return t;
        }

        void dump(auto&& field_) {
            Literal::dump(field_);
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
        size_t length = 0;

        StrLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, NodeType::str_literal), value(std::move(t)) {}

        // for decode
        StrLiteral()
            : Literal({}, NodeType::str_literal) {}

        void dump(auto&& field_) {
            Literal::dump(field_);
            sdebugf(value);
            sdebugf(length);
        }
    };

    struct BoolLiteral : Literal {
        define_node_type(NodeType::bool_literal);
        bool value = false;

        void dump(auto&& field_) {
            Literal::dump(field_);
            sdebugf(value);
        }

        BoolLiteral(lexer::Loc l, bool t)
            : Literal(l, NodeType::bool_literal), value(t) {}

        // for decode
        constexpr BoolLiteral()
            : Literal({}, NodeType::bool_literal) {}
    };

    struct TypeLiteral : Literal {
        define_node_type(NodeType::type_literal);
        define_node_description("type literal that can be used in type expressions. this is used to specify type at function arguments, field arguments, etc.");
        std::shared_ptr<Type> type_literal;
        lexer::Loc end_loc;

        void dump(auto&& field_) {
            Literal::dump(field_);
            sdebugf(type_literal);
            sdebugf(end_loc);
        }

        TypeLiteral(lexer::Loc l, const std::shared_ptr<Type>& t, lexer::Loc end_loc)
            : Literal(l, NodeType::type_literal), type_literal(t), end_loc(end_loc) {
            t->is_explicit = true;
        }

        // for decode
        constexpr TypeLiteral()
            : Literal({}, NodeType::type_literal) {}
    };

    struct SpecialLiteral : Literal {
        define_node_type(NodeType::special_literal);
        define_node_description("special literal, such as input, output, config, etc.");
        SpecialLiteralKind kind = SpecialLiteralKind::config_;

        void dump(auto&& field_) {
            Literal::dump(field_);
            sdebugf(kind);
        }

        SpecialLiteral(lexer::Loc l, const std::shared_ptr<Type>& typ, SpecialLiteralKind k)
            : Literal(l, NodeType::special_literal), kind(k) {
            this->expr_type = typ;
        }

        // for decode
        constexpr SpecialLiteral()
            : Literal({}, NodeType::special_literal) {}
    };

}  // namespace brgen::ast
