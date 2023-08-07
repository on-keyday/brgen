/*license*/
#pragma once
#include "../lexer/token.h"
#include <optional>
#include <list>
#include <number/prefix.h>
#include "expr_layer.h"
#include <escape/escape.h>
#include <map>

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
        tmp_var,
        access,
        stmt = 0x020000,
        for_,
        field,
        fmt,
        type = 0x040000,
        int_type,
        ident_type,
        str_literal_type,
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

    struct Expr : Object {
        static constexpr ObjectType object_type = ObjectType::ident_type;
        std::shared_ptr<Type> expr_type;

       protected:
        constexpr Expr(lexer::Loc l, ObjectType t)
            : Object(l, t) {}
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

    struct Literal : Expr {
       protected:
        constexpr Literal(lexer::Loc l, ObjectType t)
            : Expr(l, t) {}
    };

    using objlist = std::list<std::shared_ptr<Object>>;

    // forward declaration
    struct Fmt;
    struct Ident;
    struct Field;

    struct Definitions {
        std::map<std::string, std::list<std::shared_ptr<Fmt>>> fmts;
        std::map<std::string, std::list<std::shared_ptr<Ident>>> idents;
        std::list<std::shared_ptr<Field>> fields;

        void add_fmt(std::string& name, const std::shared_ptr<Fmt>& f) {
            fmts[name].push_back(f);
        }

        void add_ident(std::string& name, const std::shared_ptr<Ident>& f) {
            idents[name].push_back(f);
        }

        void add_field(const std::shared_ptr<Field>& f) {
            fields.push_back(f);
        }
    };

    void debug_defs(Debug& d, const Definitions& defs);

    struct IndentScope : Stmt {
        static constexpr ObjectType object_type = ObjectType::indent_scope;
        objlist elements;
        Definitions defs;
        IndentScope(lexer::Loc l)
            : Stmt(l, ObjectType::indent_scope) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("elements", [&](Debug& d) {
                    d.array([&](auto&& field) {
                        for (auto& p : elements) {
                            field([&](Debug& d) { p->debug(d); });
                        }
                    });
                });
                field("defs", [&](Debug& d) {
                    debug_defs(d, defs);
                });
            });
        }
    };

    struct Fmt : Stmt {
        static constexpr ObjectType object_type = ObjectType::fmt;
        std::string ident;
        std::shared_ptr<IndentScope> scope;
        Fmt(lexer::Loc l)
            : Stmt(l, ObjectType::fmt) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("ident", [&](Debug& d) { d.string(ident); });
                field("scope", [&](Debug& d) { scope->debug(d); });
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

    struct Field : Stmt {
        static constexpr ObjectType object_type = ObjectType::field;
        std::optional<std::string> ident;
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;

        Field(lexer::Loc l)
            : Stmt(l, ObjectType::field) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("ident", [&](Debug& d) { ident ? d.string(*ident) : d.null(); });
                field("field_type", [&](Debug& d) { field_type->debug(d); });
            });
        }
    };

    inline void debug_defs(Debug& d, const Definitions& defs) {
        d.object([&](auto&& field) {
            field("fmts", [&](Debug& d) {
                d.array([&](auto&& field) {
                    for (auto& f : defs.fmts) {
                        field([&](Debug& d) {
                            d.string(f.first);
                        });
                    }
                });
            });
            field("idents", [&](Debug& d) {
                d.array([&](auto&& field) {
                    for (auto& f : defs.idents) {
                        field([&](Debug& d) {
                            d.string(f.first);
                        });
                    }
                });
            });
            field("fields", [&](Debug& d) {
                d.array([&](auto&& field) {
                    for (auto& f : defs.fields) {
                        field([&](Debug& d) {
                            f->debug(d);
                        });
                    }
                });
            });
        });
    }

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

    struct IdentType : Type {
        static constexpr ObjectType object_type = ObjectType::ident_type;
        std::string ident;
        std::shared_ptr<Expr> arguments;
        IdentType(lexer::Loc l, std::string&& token)
            : Type(l, ObjectType::ident_type), ident(std::move(token)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("ident", [&](Debug& d) { d.string(ident); });
                field("arguments", [&](Debug& d) { arguments ? arguments->debug(d) : d.null(); });
            });
        }
    };

    std::optional<std::string> unescape(std::string_view str_lit) {
        std::string mid;
        if (!utils::escape::unescape_str(str_lit.substr(1, str_lit.size() - 2), mid)) {
            return std::nullopt;
        }
        return mid;
    }

    struct StrLiteralType : Type {
        std::string raw;
        std::optional<std::string> mid;

        StrLiteralType(lexer::Loc loc, std::string&& str)
            : Type(loc, ObjectType::str_literal_type) {}
    };

    struct Call : Expr {
        static constexpr ObjectType object_type = ObjectType::call;
        std::shared_ptr<Expr> callee;
        std::shared_ptr<Expr> arguments;
        lexer::Loc end_loc;
        Call(lexer::Loc l, std::shared_ptr<Expr>&& callee)
            : Expr(l, ObjectType::call), callee(std::move(callee)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("callee", [&](Debug& d) { callee->debug(d); });
                field("arguments", [&](Debug& d) { arguments ? arguments->debug(d) : d.null(); });
            });
        }
    };

    struct For : Stmt {
        static constexpr ObjectType object_type = ObjectType::for_;
        std::shared_ptr<IndentScope> block;

        For(lexer::Loc l)
            : Stmt(l, ObjectType::for_) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("for_block", [&](Debug& d) { block->debug(d); });
            });
        }
    };

    struct If : ExprBlock {
        static constexpr ObjectType object_type = ObjectType::if_;
        std::shared_ptr<Expr> cond;
        std::shared_ptr<IndentScope> block;
        std::shared_ptr<Object> els;

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

    struct Unary : Expr {
        static constexpr ObjectType object_type = ObjectType::unary;
        std::shared_ptr<Expr> target;
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
        std::shared_ptr<Expr> left;
        std::shared_ptr<Expr> right;
        BinaryOp op;

        Binary(lexer::Loc l, std::shared_ptr<Expr>&& left, BinaryOp op)
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

    struct Access : Expr {
        std::shared_ptr<Expr> target;
        std::string name;

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("target", [&](Debug& d) { target->debug(d); });
                field("access_to", [&](Debug& d) { d.string(name); });
            });
        }

        Access(lexer::Loc l, std::shared_ptr<Expr>&& t, std::string&& n)
            : Expr(l, ObjectType::access), target(std::move(t)), name(std::move(n)) {}
    };

    struct Cond : Expr {
        static constexpr ObjectType object_type = ObjectType::cond;
        std::shared_ptr<Expr> then;
        std::shared_ptr<Expr> cond;
        lexer::Loc els_loc;
        std::shared_ptr<Expr> els;

        Cond(lexer::Loc l, std::shared_ptr<Expr>&& then)
            : Expr(l, ObjectType::cond), then(std::move(then)) {}

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("cond", [&](Debug& d) { cond->debug(d); });
                field("then", [&](Debug& d) { then->debug(d); });
                field("else", [&](Debug& d) { els->debug(d); });
            });
        }
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
        objlist program;
        Definitions defs;

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("program", [&](Debug& d) {
                    d.array([&](auto&& field) {
                        for (auto& p : program) {
                            field([&](Debug& d) { p->debug(d); });
                        }
                    });
                });
                field("defs", [&](Debug& d) {
                    debug_defs(d, defs);
                });
            });
        }

        Program()
            : Object(lexer::Loc{}, ObjectType::program) {}
    };

    template <class T>
    constexpr T* as(auto&& t) {
        Object* v = std::to_address(t);
        if (v && v->type == T::object_type) {
            return static_cast<T*>(v);
        }
        return nullptr;
    }

    constexpr Expr* as_Expr(auto&& t) {
        Object* v = std::to_address(t);
        if (v && int(v->type) & int(ObjectType::expr)) {
            return static_cast<Expr*>(v);
        }
        return nullptr;
    }

    void traverse(auto&& t, auto&& fn) {
        Object* o = std::to_address(t);
#define SWITCH   \
    if (false) { \
    }
#define CASE(T) else if (T* v = as<T>(o))
        SWITCH
        CASE(IdentType) {
            fn(v->arguments);
        }
        CASE(Call) {
            fn(v->expr_type);
            fn(v->callee);
            fn(v->arguments);
        }
        CASE(Binary) {
            fn(v->expr_type);
            fn(v->left);
            fn(v->right);
        }
        CASE(Unary) {
            fn(v->expr_type);
            fn(v->target);
        }
        CASE(Cond) {
            fn(v->cond);
            fn(v->then);
            fn(v->els);
        }
        CASE(If) {
            fn(v->expr_type);
            fn(v->cond);
            fn(v->block);
            fn(v->els);
        }
        CASE(For) {
            fn(v->block);
        }
        CASE(Fmt) {
            fn(v->scope);
        }
        CASE(TmpVar) {
            fn(v->expr_type);
        }
        CASE(IndentScope) {
            for (auto& f : v->elements) {
                fn(f);
            }
        }
        CASE(Program) {
            for (auto& f : v->program) {
                fn(f);
            }
        }
        CASE(Field) {
            fn(v->field_type);
        }
        CASE(Access) {
            fn(v->expr_type);
            fn(v->target);
        }
#undef SWITCH
#undef CASE
    }

}  // namespace ast
