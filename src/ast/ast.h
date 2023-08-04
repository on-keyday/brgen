/*license*/
#pragma once

#include "stream.h"
#include <number/prefix.h>

namespace ast {
    enum class ObjectType {
        program,
        int_literal,
    };

    struct Object {
        lexer::Loc loc;
        const ObjectType type;

       protected:
        constexpr Object(ObjectType t)
            : type(t) {}
    };

    struct Type : Object {
       protected:
        constexpr Type(ObjectType t)
            : Object(t) {}
    };

    struct Field : Object {
    };

    struct Expr : Object {
        std::shared_ptr<Type> expr_type;

       protected:
        constexpr Expr(ObjectType t)
            : Object(t) {}
    };

    struct Literal : Expr {
       protected:
        constexpr Literal(ObjectType t)
            : Expr(t) {}
    };

    struct IntLiteral : Literal {
        std::string raw;

        template <class T>
        std::optional<T> parse_as() const {
            T t = 0;
            if (!utils::number::prefix_integer(raw, t)) {
                return std::nullopt;
            }
            return t;
        }

        IntLiteral()
            : Literal(ObjectType::int_literal) {}
    };

    struct Program : Object {
        std::list<std::shared_ptr<Object>> program;

        Program()
            : Object(ObjectType::program) {}
    };

    std::unique_ptr<Program> parse(Stream& ctx);
}  // namespace ast
