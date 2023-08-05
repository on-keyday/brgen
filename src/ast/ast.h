/*license*/
#pragma once
#include "../lexer/token.h"
#include <optional>
#include <list>
#include <number/prefix.h>

namespace ast {
    enum class ObjectType {
        program,
        expr = 0x010000,
        int_literal,
        binary,
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

    struct Binary : Expr {
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
        BinaryOp op;

        constexpr Binary()
            : Expr(ObjectType::binary) {}
    };

    struct Cond : Expr {
        std::unique_ptr<Expr> then;
        std::unique_ptr<Expr> cond;
        std::unique_ptr<Expr> els;
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
        std::list<std::unique_ptr<Object>> program;

        Program()
            : Object(ObjectType::program) {}
    };

}  // namespace ast
