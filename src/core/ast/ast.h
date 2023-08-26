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

    enum class NodeType {
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
        paren,

        // translated
        tmp_var,
        block_expr,

        stmt = 0x020000,
        for_,
        field,
        fmt,
        indent_scope,

        // translated
        assert,

        type = 0x040000,
        int_type,
        ident_type,
        int_literal_type,
        str_literal_type,
        void_type,
        bool_type,
    };

    constexpr const char* node_type_to_string(NodeType type) {
        switch (type) {
            case NodeType::program:
                return "program";
            case NodeType::expr:
                return "expr";
            case NodeType::int_literal:
                return "int_literal";
            case NodeType::bool_literal:
                return "bool_literal";
            case NodeType::binary:
                return "binary";
            case NodeType::unary:
                return "unary";
            case NodeType::cond:
                return "cond";
            case NodeType::ident:
                return "ident";
            case NodeType::call:
                return "call";
            case NodeType::if_:
                return "if";
            case NodeType::member_access:
                return "member_access";
            case NodeType::paren:
                return "paren";
            case NodeType::tmp_var:
                return "tmp_var";
            case NodeType::block_expr:
                return "block_expr";
            case NodeType::stmt:
                return "stmt";
            case NodeType::for_:
                return "for";
            case NodeType::field:
                return "field";
            case NodeType::fmt:
                return "fmt";
            case NodeType::indent_scope:
                return "indent_scope";
            case NodeType::type:
                return "type";
            case NodeType::int_type:
                return "int_type";
            case NodeType::ident_type:
                return "ident_type";
            case NodeType::int_literal_type:
                return "int_literal_type";
            case NodeType::str_literal_type:
                return "str_literal_type";
            case NodeType::void_type:
                return "void_type";
            case NodeType::bool_type:
                return "bool_type";
            default:
                return "unknown";
        }
    }

    // abstract
    struct Node {
        const NodeType type;
        lexer::Loc loc;

        virtual ~Node() {}

        virtual void as_json(Debug& buf) const {
            auto field = buf.object();
            basic_info(field);
        }

        void basic_info(auto&& field) const {
            field("node_type", node_type_to_string(type));
            field("loc", loc);
        }

       protected:
        constexpr Node(lexer::Loc l, NodeType t)
            : loc(l), type(t) {}
    };

    struct Type : Node {
       protected:
        constexpr Type(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Expr : Node {
        static constexpr NodeType node_type = NodeType::ident_type;
        std::shared_ptr<Type> expr_type;

       protected:
        constexpr Expr(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Stmt : Node {
       protected:
        constexpr Stmt(lexer::Loc l, NodeType t)
            : Node(l, t) {}
    };

    struct Literal : Expr {
       protected:
        constexpr Literal(lexer::Loc l, NodeType t)
            : Expr(l, t) {}
    };

    using node_list = std::list<std::shared_ptr<Node>>;

    struct Definitions;

    using define_frame = std::shared_ptr<StackFrame<Definitions>>;

    void debug_defs(Debug& d, const define_frame& defs);

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
        static constexpr NodeType node_type = NodeType::ident;
        std::string ident;
        define_frame frame;
        std::weak_ptr<Ident> base;
        IdentUsage usage = IdentUsage::unknown;

        Ident(lexer::Loc l, std::string&& i)
            : Expr(l, NodeType::ident), ident(std::move(i)) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(ident));
            field("usage", ident_usage_map[int(usage)]);
        }
    };

    // field
    struct Fmt;
    struct Field : Stmt {
        static constexpr NodeType node_type = NodeType::field;
        std::shared_ptr<Ident> ident;
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;
        std::shared_ptr<Expr> arguments;
        std::weak_ptr<Fmt> belong;

        Field(lexer::Loc l)
            : Stmt(l, NodeType::field) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(ident));
            field(sdebugf(colon_loc));
            field(sdebugf(field_type));
            field(sdebugf(arguments));
        }
    };

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
        static constexpr NodeType node_type = NodeType::indent_scope;
        node_list elements;
        define_frame defs;

        IndentScope(lexer::Loc l)
            : Stmt(l, NodeType::indent_scope) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(elements));
            field("defs", [&] {
                debug_defs(buf, defs);
            });
        }
    };

    struct Fmt : Stmt {
        static constexpr NodeType node_type = NodeType::fmt;
        std::string ident;
        std::shared_ptr<IndentScope> scope;
        std::weak_ptr<Fmt> belong;
        Fmt(lexer::Loc l)
            : Stmt(l, NodeType::fmt) {}

        std::string ident_path(const char* sep = "_") {
            if (auto parent = belong.lock()) {
                return parent->ident_path() + sep + ident;
            }
            return ident;
        }

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(ident));
            field(sdebugf(scope));
        }
    };

    struct For : Stmt {
        static constexpr NodeType node_type = NodeType::for_;
        std::shared_ptr<IndentScope> block;

        For(lexer::Loc l)
            : Stmt(l, NodeType::for_) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field("for_block", block);
        }
    };

    // exprs

    struct Call : Expr {
        static constexpr NodeType node_type = NodeType::call;
        std::shared_ptr<Expr> callee;
        std::shared_ptr<Expr> arguments;
        lexer::Loc end_loc;
        Call(lexer::Loc l, std::shared_ptr<Expr>&& callee)
            : Expr(l, NodeType::call), callee(std::move(callee)) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(callee));
            field(sdebugf(arguments));
            field(sdebugf(end_loc));
        }
    };

    struct Paren : Expr {
        static constexpr NodeType node_type = NodeType::paren;
        std::shared_ptr<Expr> expr;
        lexer::Loc end_loc;
        Paren(lexer::Loc l)
            : Expr(l, NodeType::paren) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(expr));
            field(sdebugf(end_loc));
        }
    };

    struct If : Expr {
        static constexpr NodeType node_type = NodeType::if_;
        std::shared_ptr<Expr> cond;
        std::shared_ptr<IndentScope> block;
        std::shared_ptr<Node> els;

        constexpr If(lexer::Loc l)
            : Expr(l, NodeType::if_) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(cond));
            field(sdebugf(block));
            field(sdebugf(els));
        }
    };

    struct Unary : Expr {
        static constexpr NodeType node_type = NodeType::unary;
        std::shared_ptr<Expr> target;
        UnaryOp op;

        constexpr Unary(lexer::Loc l, UnaryOp p)
            : Expr(l, NodeType::unary), op(p) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            auto op = unary_op[int(this->op)];
            field(sdebugf(op));
            field(sdebugf(target));
        }
    };

    struct Binary : Expr {
        static constexpr NodeType node_type = NodeType::binary;
        std::shared_ptr<Expr> left;
        std::shared_ptr<Expr> right;
        BinaryOp op;

        Binary(lexer::Loc l, std::shared_ptr<Expr>&& left, BinaryOp op)
            : Expr(l, NodeType::binary), left(std::move(left)), op(op) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            auto op = bin_op_str(this->op);
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(op));
            field(sdebugf(left));
            field(sdebugf(right));
        }
    };

    struct MemberAccess : Expr {
        static constexpr NodeType node_type = NodeType::member_access;
        std::shared_ptr<Expr> target;
        std::string name;

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(target));
            field(sdebugf(name));
        }

        MemberAccess(lexer::Loc l, std::shared_ptr<Expr>&& t, std::string&& n)
            : Expr(l, NodeType::member_access), target(std::move(t)), name(std::move(n)) {}
    };

    struct Cond : Expr {
        static constexpr NodeType node_type = NodeType::cond;
        std::shared_ptr<Expr> then;
        std::shared_ptr<Expr> cond;
        lexer::Loc els_loc;
        std::shared_ptr<Expr> els;

        Cond(lexer::Loc l, std::shared_ptr<Expr>&& then)
            : Expr(l, NodeType::cond), then(std::move(then)) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(cond));
            field(sdebugf(then));
            field(sdebugf(els_loc));
            field(sdebugf(els));
        }
    };

    // literals
    struct IntLiteral : Literal {
        static constexpr NodeType node_type = NodeType::int_literal;
        std::string raw;

        template <class T>
        std::optional<T> parse_as() const {
            T t = 0;
            if (!utils::number::prefix_integer(raw, t)) {
                return std::nullopt;
            }
            return t;
        }

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(raw));
            auto num = parse_as<std::int64_t>();
            field(sdebugf(num));
        }

        IntLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, NodeType::int_literal), raw(std::move(t)) {}
    };

    struct BoolLiteral : Literal {
        static constexpr NodeType node_type = NodeType::bool_literal;
        bool value;

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(expr_type));
            field(sdebugf(value));
        }

        BoolLiteral(lexer::Loc l, bool t)
            : Literal(l, NodeType::bool_literal), value(t) {}
    };

    // types

    constexpr std::uint8_t aligned_bit(size_t bit) {
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
        return 0;
    }

    struct IntegerType : Type {
        static constexpr NodeType node_type = NodeType::int_type;
        std::string raw;
        size_t bit_size = 0;

        IntegerType(lexer::Loc l, std::string&& token, size_t bit_size)
            : Type(l, NodeType::int_type), raw(std::move(token)), bit_size(bit_size) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(raw));
            field(sdebugf(bit_size));
        }
    };

    struct IntLiteralType : Type {
        static constexpr NodeType node_type = NodeType::int_literal_type;
        std::weak_ptr<IntLiteral> base;
        mutable std::optional<std::uint8_t> bit_size;

        std::optional<std::uint8_t> get_bit_size() const {
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

        std::uint8_t get_aligned_bit() const {
            if (auto s = get_bit_size()) {
                return aligned_bit(*s);
            }
            return 0;
        }

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            auto bit_size = get_bit_size();
            auto aligned = get_aligned_bit();
            basic_info(field);
            field(sdebugf(bit_size));
            field(sdebugf(aligned));
        }

        IntLiteralType(const std::shared_ptr<IntLiteral>& ty)
            : Type(ty->loc, NodeType::int_literal_type), base(ty) {}
    };

    struct IdentType : Type {
        static constexpr NodeType node_type = NodeType::ident_type;
        std::string ident;
        define_frame frame;
        IdentType(lexer::Loc l, std::string&& token, define_frame&& frame)
            : Type(l, NodeType::ident_type), ident(std::move(token)), frame(std::move(frame)) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(ident));
        }
    };

    struct VoidType : Type {
        static constexpr NodeType node_type = NodeType::void_type;

        VoidType(lexer::Loc l)
            : Type(l, NodeType::void_type) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
        }
    };

    struct BoolType : Type {
        static constexpr NodeType node_type = NodeType::bool_type;

        BoolType(lexer::Loc l)
            : Type(l, NodeType::bool_type) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
        }
    };

    struct StrLiteralType : Type {
        std::string raw;
        std::optional<std::string> mid;

        StrLiteralType(lexer::Loc loc, std::string&& str)
            : Type(loc, NodeType::str_literal_type) {}

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(raw));
            field(sdebugf(mid));
        }
    };

    void debug_def_frames(Debug& d, const define_frame& f);

    struct Program : Node {
        static constexpr NodeType node_type = NodeType::program;
        node_list elements;
        define_frame defs;

        void as_json(Debug& buf) const override {
            auto field = buf.object();
            basic_info(field);
            field(sdebugf(elements));
            field("defs", [&] { debug_defs(buf, defs); });
            field("all_defs", [&] { debug_def_frames(buf, defs); });
        }

        Program()
            : Node(lexer::Loc{}, NodeType::program) {}
    };

    inline void debug_defs(Debug& d, const define_frame& defs) {
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

    inline void debug_def_frames(Debug& d, const define_frame& f) {
        auto field = d.array();
        for (auto frame = f; frame; frame = frame->next_frame()) {
            field([&] { debug_defs(d, frame); });
            if (auto branch = frame->branch_frame()) {
                field([&] { debug_def_frames(d, branch); });
            }
        }
    }

    template <class T>
    constexpr T* as(auto&& t) {
        Node* v = std::to_address(t);
        if (v && v->type == T::node_type) {
            return static_cast<T*>(v);
        }
        return nullptr;
    }

    constexpr Expr* as_Expr(auto&& t) {
        Node* v = std::to_address(t);
        if (v && int(v->type) & int(NodeType::expr)) {
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
