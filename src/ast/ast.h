/*license*/
#pragma once
#include "../lexer/token.h"
#include <optional>
#include <list>
#include <number/prefix.h>
#include "expr_layer.h"
#include <vector>
#include <escape/escape.h>

namespace ast {

    auto nums(auto v, int radix = 10) {
        return utils::number::to_string<std::string>(v, radix);
    }

    using utils::strutil::append, utils::strutil::appends;

    struct Debug {
        std::string buf;
        void object(auto&& fn) {
            append(buf, "{");
            bool comma = false;
            fn([](std::string_view name, auto&& value) {
                if (comma) {
                    append(buf, ",");
                }
                append(buf, "\"", name, "\": ");
                value(*this);
                comma = true;
            });
            append(buf, "}");
        }

        void array(auto&& fn) {
            append(buf, "[");
            bool comma = false;
            fn([&](auto&& value) {
                if (comma) {
                    append(buf, ",");
                }
                value(*this);
                comma = true;
            });
            append(buf, "]");
        }

        void number(std::int64_t value) {
            append(buf, nums(value));
        }

        void string(std::string_view s) {
            appends(buf, "\"", utils::escape::escape_str<std::string>(s, utils::escape::EscapeFlag::all, utils::escape::json_set()), "\"");
        }

        void null() {
            appends(buf, "null");
        }

        void boolean(bool v) {
            append(buf, v ? "true" : "false");
        }
    };

    enum class ObjectType {
        program,
        expr = 0x010000,
        int_literal,
        binary,
        unary,
        cond,
        ident,
        if_,
        indent_scope,
    };

    struct Object {
        lexer::Loc loc;
        const ObjectType type;
        virtual ~Object() {}

        virtual void debug(Debug& buf) const {
            buf.null();
        }

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

    struct Block : Expr {
       protected:
        constexpr Block(lexer::Loc l, ObjectType t)
            : Expr(l, t) {}
    };

    struct IndentScope : Block {
        std::vector<std::unique_ptr<Object>> elements;
        IndentScope(lexer::Loc l)
            : Block(l, ObjectType::indent_scope) {}

        void debug(Debug& buf) const override {
            buf.array([&](auto&& field) {
                for (auto& p : elements) {
                    field([&](Debug& d) { p->debug(d); });
                }
            });
        }
    };

    struct If : Block {
        std::unique_ptr<Expr> cond;
        std::unique_ptr<Block> block;
        std::unique_ptr<Block> els;

        constexpr If(lexer::Loc l)
            : Block(l, ObjectType::if_) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("cond", [&](Debug& d) { cond->debug(d); });
                field("block", [&](Debug& d) { block->debug(d); });
                field("else", [&](Debug& d) { els->debug(d); });
            });
        }
    };

    struct Ident : Expr {
        std::string ident;

        Ident(lexer::Loc l, std::string&& i)
            : Expr(l, ObjectType::ident), ident(std::move(i)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("ident", [&](Debug& d) { d.string(ident); });
            });
        }
    };

    struct Unary : Expr {
        std::unique_ptr<Expr> target;
        UnaryOp op;

        constexpr Unary(lexer::Loc l, UnaryOp p)
            : Expr(l, ObjectType::unary), op(p) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("target", [&](Debug& d) { target->debug(d); });
                field("op", [&](Debug& d) { d.string(unary_op[int(op)]); });
            });
        }
    };

    struct Binary : Expr {
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
        BinaryOp op;

        Binary(lexer::Loc l, std::unique_ptr<Expr>&& left, BinaryOp op)
            : Expr(l, ObjectType::binary), left(std::move(left)), op(op) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("left", [&](Debug& d) { then->debug(d); });
                field("right", [&](Debug& d) { cond->debug(d); });
                auto c = bin_op_str(op);
                field("op", [&](Debug& d) { c ? d.string(*c) : d.null(); });
            });
        }
    };

    struct Cond : Expr {
        std::unique_ptr<Expr> then;
        std::unique_ptr<Expr> cond;
        lexer::Loc els_loc;
        std::unique_ptr<Expr> els;

        Cond(lexer::Loc l, std::unique_ptr<Expr>&& then)
            : Expr(l, ObjectType::cond), then(std::move(then)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("then", [&](Debug& d) { then->debug(d); });
                field("cond", [&](Debug& d) { cond->debug(d); });
                field("els", [&](Debug& d) { els->debug(d); });
            });
        }
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

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                auto p = parse_as<std::int64_t>();
                field("raw", [&](Debug& d) { d.string(raw) });
                field("num", [&](Debug& d) { p ? d.string(*p) : d.null(); });
            });
        }

        IntLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, ObjectType::int_literal), raw(std::move(t)) {}
    };

    struct Program : Object {
        std::list<std::unique_ptr<Object>> program;

        void debug(Debug& buf) const override {
            buf.array([&](auto&& field) {
                for (auto& p : program) {
                    field([&](Debug& d) { p->debug(d); });
                }
            });
        }

        Program()
            : Object(lexer::Loc{}, ObjectType::program) {}
    };

}  // namespace ast
