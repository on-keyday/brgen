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
            fn([&](std::string_view name, auto&& value) {
                if (comma) {
                    append(buf, ",");
                }
                appends(buf, "\"", name, "\": ");
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
        call,
        if_,
        indent_scope,
        stmt = 0x020000,
        for_,
        field,
        type = 0x040000,
        int_type,
        ident_type,
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

    struct IntegerType : Type {
        static constexpr ObjectType object_type = ObjectType::int_type;
        std::string raw;
        size_t bit_size = 0;

        IntegerType(lexer::Loc l, std::string&& token, size_t bit_size)
            : Type(l, ObjectType::int_type), raw(std::move(token)), bit_size(bit_size) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("raw", [&](Debug& d) { d.string(raw); });
                field("bit_size", [&](Debug& d) { d.number(bit_size); });
            });
        }
    };

    struct Expr : Object {
        static constexpr ObjectType object_type = ObjectType::ident_type;
        std::shared_ptr<Type> expr_type;

       protected:
        constexpr Expr(lexer::Loc l, ObjectType t)
            : Object(l, t) {}
    };

    struct Expr;

    struct IdentType : Type {
        static constexpr ObjectType object_type = ObjectType::ident;
        std::string ident;
        std::unique_ptr<Expr> arguments;
        IdentType(lexer::Loc l, std::string&& token)
            : Type(l, ObjectType::ident_type), ident(std::move(token)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("ident", [&](Debug& d) { d.string(ident); });
                field("arguments", [&](Debug& d) { arguments ? arguments->debug(d) : d.null(); });
            });
        }
    };

    struct Call : Expr {
        static constexpr ObjectType object_type = ObjectType::call;
        std::unique_ptr<Expr> callee;
        std::unique_ptr<Expr> arguments;
        lexer::Loc end_loc;
        Call(lexer::Loc l, std::unique_ptr<Expr>&& callee)
            : Expr(l, ObjectType::call), callee(std::move(callee)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("callee", [&](Debug& d) { callee->debug(d); });
                field("arguments", [&](Debug& d) { arguments ? arguments->debug(d) : d.null(); });
            });
        }
    };

    struct ExprBlock : Expr {
       protected:
        constexpr ExprBlock(lexer::Loc l, ObjectType t)
            : Expr(l, t) {}
    };

    struct Stmt : Object {
       protected:
        constexpr Stmt(lexer::Loc l, ObjectType t)
            : Object(l, t) {}
    };

    struct Field : Stmt {
        static constexpr ObjectType object_type = ObjectType::field;
        std::optional<std::string> ident;
        lexer::Loc colon_loc;
        std::unique_ptr<Type> field_type;

        Field(lexer::Loc l)
            : Stmt(l, ObjectType::field) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("ident", [&](Debug& d) { ident ? d.string(*ident) : d.null(); });
                field("field_type", [&](Debug& d) { field_type->debug(d); });
            });
        }
    };

    struct IndentScope : Stmt {
        static constexpr ObjectType object_type = ObjectType::indent_scope;
        std::vector<std::unique_ptr<Object>> elements;
        IndentScope(lexer::Loc l)
            : Stmt(l, ObjectType::indent_scope) {}

        void debug(Debug& buf) const override {
            buf.array([&](auto&& field) {
                for (auto& p : elements) {
                    field([&](Debug& d) { p->debug(d); });
                }
            });
        }
    };

    struct For : Stmt {
        static constexpr ObjectType object_type = ObjectType::for_;
        std::unique_ptr<IndentScope> block;

        For(lexer::Loc l)
            : Stmt(l, ObjectType::for_) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("block", [&](Debug& d) { block->debug(d); });
            });
        }
    };

    struct If : ExprBlock {
        static constexpr ObjectType object_type = ObjectType::if_;
        std::unique_ptr<Expr> cond;
        std::unique_ptr<IndentScope> block;
        std::unique_ptr<Object> els;

        constexpr If(lexer::Loc l)
            : ExprBlock(l, ObjectType::if_) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("cond", [&](Debug& d) { cond->debug(d); });
                field("block", [&](Debug& d) { block->debug(d); });
                field("else", [&](Debug& d) { els ? els->debug(d) : d.null(); });
            });
        }
    };

    struct Ident : Expr {
        static constexpr ObjectType object_type = ObjectType::ident;
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
        static constexpr ObjectType object_type = ObjectType::unary;
        std::unique_ptr<Expr> target;
        UnaryOp op;

        constexpr Unary(lexer::Loc l, UnaryOp p)
            : Expr(l, ObjectType::unary), op(p) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("op", [&](Debug& d) { d.string(unary_op[int(op)]); });
                field("target", [&](Debug& d) { target->debug(d); });
            });
        }
    };

    struct Binary : Expr {
        static constexpr ObjectType object_type = ObjectType::binary;
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
        BinaryOp op;

        Binary(lexer::Loc l, std::unique_ptr<Expr>&& left, BinaryOp op)
            : Expr(l, ObjectType::binary), left(std::move(left)), op(op) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                auto c = bin_op_str(op);
                field("op", [&](Debug& d) { c ? d.string(*c) : d.null(); });
                field("left", [&](Debug& d) { left->debug(d); });
                field("right", [&](Debug& d) { right->debug(d); });
            });
        }
    };

    struct Cond : Expr {
        static constexpr ObjectType object_type = ObjectType::cond;
        std::unique_ptr<Expr> then;
        std::unique_ptr<Expr> cond;
        lexer::Loc els_loc;
        std::unique_ptr<Expr> els;

        Cond(lexer::Loc l, std::unique_ptr<Expr>&& then)
            : Expr(l, ObjectType::cond), then(std::move(then)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("cond", [&](Debug& d) { cond->debug(d); });
                field("then", [&](Debug& d) { then->debug(d); });
                field("else", [&](Debug& d) { els->debug(d); });
            });
        }
    };

    struct Literal : Expr {
       protected:
        constexpr Literal(lexer::Loc l, ObjectType t)
            : Expr(l, t) {}
    };

    struct IntLiteral : Literal {
        static constexpr ObjectType object_type = ObjectType::int_literal;
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
                field("raw", [&](Debug& d) { d.string(raw); });
                field("num", [&](Debug& d) { p ? d.number(*p) : d.null(); });
            });
        }

        IntLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, ObjectType::int_literal), raw(std::move(t)) {}
    };

    struct Program : Object {
        static constexpr ObjectType object_type = ObjectType::program;
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

    template <class T>
    constexpr T* as(Object* t) {
        if (t && t->type == T::object_type) {
            return static_cast<T*>(t);
        }
        return nullptr;
    }

}  // namespace ast
