/*license*/
#pragma once
#include "../lexer/token.h"
#include <optional>
#include <list>
#include <number/prefix.h>
#include "expr_layer.h"

namespace ast {
    enum class ObjectType {
        program,
        expr = 0x010000,
        int_literal,
        binary,
        unary,
        cond,
        ident,
    };

    struct Object {
        lexer::Loc loc;
        const ObjectType type;
        virtual ~Object() {}

       protected:
        constexpr Object(lexer::Loc l, ObjectType t)
            : loc(l), type(t) {}
    };

    struct Type : Object {
       protected:
        constexpr Type(lexer::Loc l, ObjectType t)
            : Object(l, t) {}
    };

    struct Field : Object {
    };

    struct Expr : Object {
        std::shared_ptr<Type> expr_type;

       protected:
        constexpr Expr(lexer::Loc l, ObjectType t)
            : Object(l, t) {}
    };

    struct Ident : Expr {
        std::string ident;

        Ident(lexer::Loc l, std::string&& i)
            : Expr(l, ObjectType::ident), ident(std::move(i)) {}
    };

    struct Unary : Expr {
        std::unique_ptr<Expr> target;
        UnaryOp op;

        constexpr Unary(lexer::Loc l, UnaryOp p)
            : Expr(l, ObjectType::unary), op(p) {}
    };

    struct Binary : Expr {
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
        BinaryOp op;

        Binary(lexer::Loc l, std::unique_ptr<Expr>&& left, BinaryOp op)
            : Expr(l, ObjectType::binary), left(std::move(left)), op(op) {}
    };

    struct Cond : Expr {
        std::unique_ptr<Expr> then;
        std::unique_ptr<Expr> cond;
        lexer::Loc els_loc;
        std::unique_ptr<Expr> els;

        Cond(lexer::Loc l, std::unique_ptr<Expr>&& then)
            : Expr(l, ObjectType::cond), then(std::move(then)) {}
    };

    struct Literal : Expr {
       protected:
        constexpr Literal(lexer::Loc l, ObjectType t)
            : Expr(l, t) {}
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

        IntLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, ObjectType::int_literal), raw(std::move(t)) {}
    };

    struct Program : Object {
        std::list<std::unique_ptr<Object>> program;

        Program()
            : Object(lexer::Loc{}, ObjectType::program) {}
    };

}  // namespace ast
