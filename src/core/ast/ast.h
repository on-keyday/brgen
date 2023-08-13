/*license*/
#pragma once
#include "../lexer/token.h"
#include <optional>
#include <list>
#include <number/prefix.h>
#include "expr_layer.h"
#include <escape/escape.h>
#include <map>
#include "../common/stack.h"
#include "../common/util.h"
#include "../common/debug.h"
#include <binary/flags.h>

namespace brgen::ast {

    enum class ObjectType {
        program,
        expr = 0x010000,
        int_literal,
        bool_literal,
        binary,
        unary,
        cond,
        ident,
        call,
        if_,
        member_access,

        // translated
        tmp_var,
        block_expr,

        stmt = 0x020000,
        for_,
        field,
        fmt,
        indent_scope,
        type = 0x040000,
        int_type,
        ident_type,
        int_literal_type,
        str_literal_type,
        void_type,
        bool_type,
    };

    // abstract
    struct Node {
        const ObjectType type;
        lexer::Loc loc;

        virtual ~Node() {}

        virtual void debug(Debug& buf) const {
            buf.null();
        }

       protected:
        constexpr Node(lexer::Loc l, ObjectType t)
            : loc(l), type(t) {}
    };

    struct Type : Node {
       protected:
        constexpr Type(lexer::Loc l, ObjectType t)
            : Node(l, t) {}
    };

    struct Expr : Node {
        static constexpr ObjectType object_type = ObjectType::ident_type;
        std::shared_ptr<Type> expr_type;

       protected:
        constexpr Expr(lexer::Loc l, ObjectType t)
            : Node(l, t) {}
    };

    struct Stmt : Node {
       protected:
        constexpr Stmt(lexer::Loc l, ObjectType t)
            : Node(l, t) {}
    };

    struct Literal : Expr {
       protected:
        constexpr Literal(lexer::Loc l, ObjectType t)
            : Expr(l, t) {}
    };

    using node_list = std::list<std::shared_ptr<Node>>;

    struct Definitions;

    using defframe = std::shared_ptr<StackFrame<Definitions>>;

    void debug_defs(Debug& d, const defframe& defs);

    // ident

    enum class IdentUsage {
        unknown,
        reference,
        define_alias,
        define_typed,
        define_const,
    };

    constexpr const char* ident_usage_map[]{
        "unknown",
        "reference",
        "define_alias",
        "define_typed",
        "define_const",
    };

    struct Ident : Expr {
        static constexpr ObjectType object_type = ObjectType::ident;
        std::string ident;
        defframe frame;
        std::weak_ptr<Ident> base;
        IdentUsage usage = IdentUsage::unknown;

        Ident(lexer::Loc l, std::string&& i)
            : Expr(l, ObjectType::ident), ident(std::move(i)) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(ident));
            field("usage", ident_usage_map[int(usage)]);
        }
    };

    // field

    struct Field : Stmt {
        static constexpr ObjectType object_type = ObjectType::field;
        std::shared_ptr<Ident> ident;
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;
        std::shared_ptr<Expr> arguments;

        Field(lexer::Loc l)
            : Stmt(l, ObjectType::field) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(ident));
            field(sdebugf(field_type));
            field(sdebugf(arguments));
        }
    };

    struct Fmt;

    struct Definitions {
        std::map<std::string, std::list<std::shared_ptr<Fmt>>> fmts;
        std::map<std::string, std::list<std::shared_ptr<Ident>>> idents;
        std::list<std::shared_ptr<Field>> fields;
        node_list order;

        void add_fmt(std::string& name, const std::shared_ptr<Fmt>& f);

        void add_ident(std::string& name, const std::shared_ptr<Ident>& f);

        void add_field(const std::shared_ptr<Field>& f);
    };

    // statements

    struct IndentScope : Stmt {
        static constexpr ObjectType object_type = ObjectType::indent_scope;
        node_list elements;
        defframe defs;

        IndentScope(lexer::Loc l)
            : Stmt(l, ObjectType::indent_scope) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(elements));
            field("defs", [&] {
                debug_defs(buf, defs);
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
            auto field = buf.object();
            field(sdebugf(ident));
            field(sdebugf(scope));
        }
    };

    struct For : Stmt {
        static constexpr ObjectType object_type = ObjectType::for_;
        std::shared_ptr<IndentScope> block;

        For(lexer::Loc l)
            : Stmt(l, ObjectType::for_) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field("for_block", block);
        }
    };

    // exprs

    struct Call : Expr {
        static constexpr ObjectType object_type = ObjectType::call;
        std::shared_ptr<Expr> callee;
        std::shared_ptr<Expr> arguments;
        lexer::Loc end_loc;
        Call(lexer::Loc l, std::shared_ptr<Expr>&& callee)
            : Expr(l, ObjectType::call), callee(std::move(callee)) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(callee));
            field(sdebugf(arguments));
        }
    };

    struct If : Expr {
        static constexpr ObjectType object_type = ObjectType::if_;
        std::shared_ptr<Expr> cond;
        std::shared_ptr<IndentScope> block;
        std::shared_ptr<Node> els;

        constexpr If(lexer::Loc l)
            : Expr(l, ObjectType::if_) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(cond));
            field(sdebugf(block));
            field(sdebugf(els));
        }
    };

    struct Unary : Expr {
        static constexpr ObjectType object_type = ObjectType::unary;
        std::shared_ptr<Expr> target;
        UnaryOp op;

        constexpr Unary(lexer::Loc l, UnaryOp p)
            : Expr(l, ObjectType::unary), op(p) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            auto op = unary_op[int(this->op)];
            field(sdebugf(op));
            field(sdebugf(target));
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
            auto field = buf.object();
            auto op = bin_op_str(this->op);
            field(sdebugf(op));
            field(sdebugf(left));
            field(sdebugf(right));
        }
    };

    struct MemberAccess : Expr {
        static constexpr ObjectType object_type = ObjectType::member_access;
        std::shared_ptr<Expr> target;
        std::string name;

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(target));
            field(sdebugf(name));
        }

        MemberAccess(lexer::Loc l, std::shared_ptr<Expr>&& t, std::string&& n)
            : Expr(l, ObjectType::member_access), target(std::move(t)), name(std::move(n)) {}
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
            auto field = buf.object();
            field(sdebugf(cond));
            field(sdebugf(then));
            field(sdebugf(els));
        }
    };

    // literals
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
            auto field = buf.object();
            field(sdebugf(raw));
            auto num = parse_as<std::int64_t>();
            field(sdebugf(num));
        }

        IntLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, ObjectType::int_literal), raw(std::move(t)) {}
    };

    struct BoolLiteral : Literal {
        static constexpr ObjectType object_type = ObjectType::bool_literal;
        bool value;
        BoolLiteral(lexer::Loc l, bool t)
            : Literal(l, ObjectType::bool_literal), value(t) {}
    };

    // types

    struct IntegerType : Type {
        static constexpr ObjectType object_type = ObjectType::int_type;
        std::string raw;
        size_t bit_size = 0;

        IntegerType(lexer::Loc l, std::string&& token, size_t bit_size)
            : Type(l, ObjectType::int_type), raw(std::move(token)), bit_size(bit_size) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(raw));
            field(sdebugf(bit_size));
        }
    };

    struct IntLiteralType : Type {
        static constexpr ObjectType object_type = ObjectType::int_literal_type;
        std::weak_ptr<IntLiteral> base;
        std::optional<std::uint8_t> bit_size;

        std::optional<std::uint8_t> get_bit_size() {
            if (bit_size) {
                return bit_size;
            }
            auto got = base.lock();
            if (!got) {
                return std::nullopt;
            }
            auto p = got->parse_as<std::uint64_t>();
            if (!p) {
                return std::nullopt;
            }
            bit_size = utils::binary::log2i(*p);
            return bit_size;
        }

        std::uint8_t aligned_bit() {
            if (auto s = get_bit_size()) {
                auto bit = *s;
                if (bit <= 8) {
                    return 8;
                }
                else if (bit <= 16) {
                    return 16;
                }
                else if (bit <= 32) {
                    return 32;
                }
                else if (bit <= 64) {
                    return 64;
                }
            }
            return 0;
        }

        IntLiteralType(const std::shared_ptr<IntLiteral>& ty)
            : Type(ty->loc, ObjectType::int_literal_type), base(ty) {}
    };

    struct IdentType : Type {
        static constexpr ObjectType object_type = ObjectType::ident_type;
        std::string ident;
        defframe frame;
        IdentType(lexer::Loc l, std::string&& token, defframe&& frame)
            : Type(l, ObjectType::ident_type), ident(std::move(token)), frame(std::move(frame)) {}

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(ident));
        }
    };

    struct VoidType : Type {
        static constexpr ObjectType object_type = ObjectType::void_type;

        VoidType(lexer::Loc l)
            : Type(l, ObjectType::void_type) {}

        void debug(Debug& buf) const override {
            buf.string("void");
        }
    };

    struct BoolType : Type {
        static constexpr ObjectType object_type = ObjectType::bool_type;

        BoolType(lexer::Loc l)
            : Type(l, ObjectType::bool_type) {}

        void debug(Debug& buf) const override {
            buf.string("bool");
        }
    };

    struct StrLiteralType : Type {
        std::string raw;
        std::optional<std::string> mid;

        StrLiteralType(lexer::Loc loc, std::string&& str)
            : Type(loc, ObjectType::str_literal_type) {}
    };

    void debug_def_frames(Debug& d, const defframe& f);

    struct Program : Node {
        static constexpr ObjectType object_type = ObjectType::program;
        node_list elements;
        defframe defs;

        void debug(Debug& buf) const override {
            auto field = buf.object();
            field(sdebugf(elements));
            field("defs", [&] { debug_defs(buf, defs); });
            field("all_defs", [&] { debug_def_frames(buf, defs); });
        }

        Program()
            : Node(lexer::Loc{}, ObjectType::program) {}
    };

    inline void debug_defs(Debug& d, const defframe& defs) {
        auto field = d.object();
        field("fmts", [&] {
            auto field = d.array();
            for (auto& f : defs->current.fmts) {
                field(f.first);
            }
        });
        field("idents", [&] {
            auto field = d.array();
            for (auto& f : defs->current.idents) {
                field(f.first);
            }
        });
        field("fields", defs->current.fields);
    }

    inline void debug_def_frames(Debug& d, const defframe& f) {
        auto field = d.object();
        field("current", [&] {
            debug_defs(d, f);
        });
        f->walk_frames([&](const char* obj, const defframe& f) {
            field(obj, [&] {
                f ? debug_def_frames(d, f) : d.null();
            });
        });
    }

    template <class T>
    constexpr T* as(auto&& t) {
        Node* v = std::to_address(t);
        if (v && v->type == T::object_type) {
            return static_cast<T*>(v);
        }
        return nullptr;
    }

    constexpr Expr* as_Expr(auto&& t) {
        Node* v = std::to_address(t);
        if (v && int(v->type) & int(ObjectType::expr)) {
            return static_cast<Expr*>(v);
        }
        return nullptr;
    }

    inline void Definitions::add_fmt(std::string& name, const std::shared_ptr<Fmt>& f) {
        fmts[name].push_back(f);
        order.push_back(f);
    }

    inline void Definitions::add_ident(std::string& name, const std::shared_ptr<Ident>& f) {
        idents[name].push_back(f);
        order.push_back(f);
    }

    inline void Definitions::add_field(const std::shared_ptr<Field>& f) {
        fields.push_back(f);
        order.push_back(f);
    }

}  // namespace brgen::ast
