/*license*/
#pragma once
#include <core/lexer/token.h>
#include <optional>
#include <list>
#include <number/prefix.h>
#include "../expr_layer.h"
#include <escape/escape.h>
#include <map>
#include <core/common/stack.h>
#include <core/common/util.h>
#include <core/common/debug.h>
#include <binary/flags.h>
#include <binary/log2i.h>
#include "base.h"

namespace brgen::ast {

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

    constexpr void as_json(IdentUsage usage, auto&& buf) {
        buf.string(ident_usage_map[int(usage)]);
    }

    struct Ident : Expr {
        define_node_type(NodeType::ident);
        std::string ident;
        define_frame frame;
        std::weak_ptr<Ident> base;
        IdentUsage usage = IdentUsage::unknown;

        Ident(lexer::Loc l, std::string&& i)
            : Expr(l, NodeType::ident), ident(std::move(i)) {}

        // for decode
        Ident()
            : Expr({}, NodeType::ident) {}

        void dump(auto&& field) {
            dump(field);
        }

        void dump(auto&& field) const {
            Expr::dump(field);
            field(sdebugf(ident));
            field(sdebugf(usage));
        }
    };

    // field
    struct Fmt;
    struct Field : Stmt {
        define_node_type(NodeType::field);
        std::shared_ptr<Ident> ident;
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;
        std::shared_ptr<Expr> arguments;
        std::weak_ptr<Fmt> belong;

        Field(lexer::Loc l)
            : Stmt(l, NodeType::field) {}

        // for decode
        Field()
            : Stmt({}, NodeType::field) {}

        void dump(auto&& field) {
            dump(field);
        }

        void dump(auto&& field) const {
            Stmt::dump(field);
            field(sdebugf(ident));
            field(sdebugf(colon_loc));
            field(sdebugf(field_type));
            field(sdebugf(arguments));
        }
    };

    struct Function;

    struct Definitions {
        std::map<std::string, std::list<std::shared_ptr<Fmt>>> fmts;
        std::map<std::string, std::list<std::shared_ptr<Ident>>> idents;
        std::list<std::shared_ptr<Field>> fields;
        std::map<std::string, std::shared_ptr<Function>> funcs;
        node_list order;

        void add_fmt(std::string& name, const std::shared_ptr<Fmt>& f);

        void add_ident(std::string& name, const std::shared_ptr<Ident>& f);

        void add_field(const std::shared_ptr<Field>& f);
    };

    // statements

    struct IndentScope : Stmt {
        define_node_type(NodeType::indent_scope);
        node_list elements;
        define_frame defs;

        IndentScope(lexer::Loc l)
            : Stmt(l, NodeType::indent_scope) {}

        // for decode
        IndentScope()
            : Stmt({}, NodeType::indent_scope) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(elements));
            field("defs", [&](Debug& buf) {
                debug_defs(buf, defs);
            });
        }
    };

    struct Fmt : Stmt {
        define_node_type(NodeType::fmt);
        std::string ident;
        std::shared_ptr<IndentScope> scope;
        std::weak_ptr<Fmt> belong;
        Fmt(lexer::Loc l)
            : Stmt(l, NodeType::fmt) {}

        // for decode
        Fmt()
            : Stmt({}, NodeType::fmt) {}

        std::string ident_path(const char* sep = "_") {
            if (auto parent = belong.lock()) {
                return parent->ident_path() + sep + ident;
            }
            return ident;
        }

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(ident));
            field(sdebugf(scope));
        }
    };

    struct For : Stmt {
        define_node_type(NodeType::for_);
        std::shared_ptr<IndentScope> block;

        For(lexer::Loc l)
            : Stmt(l, NodeType::for_) {}

        For()
            : Stmt({}, NodeType::for_) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field("for_block", block);
        }
    };

    // exprs

    struct Call : Expr {
        define_node_type(NodeType::call);
        std::shared_ptr<Expr> callee;
        std::shared_ptr<Expr> arguments;
        lexer::Loc end_loc;
        Call(lexer::Loc l, std::shared_ptr<Expr>&& callee)
            : Expr(l, NodeType::call), callee(std::move(callee)) {}
        Call()
            : Expr({}, NodeType::call) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(expr_type));
            field(sdebugf(callee));
            field(sdebugf(arguments));
            field(sdebugf(end_loc));
        }
    };

    struct Paren : Expr {
        define_node_type(NodeType::paren);
        std::shared_ptr<Expr> expr;
        lexer::Loc end_loc;
        Paren(lexer::Loc l)
            : Expr(l, NodeType::paren) {}

        // for decode
        Paren()
            : Expr({}, NodeType::paren) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(expr_type));
            field(sdebugf(expr));
            field(sdebugf(end_loc));
        }
    };

    struct If : Expr {
        define_node_type(NodeType::if_);
        std::shared_ptr<Expr> cond;
        std::shared_ptr<IndentScope> block;
        std::shared_ptr<Node> els;

        constexpr If(lexer::Loc l)
            : Expr(l, NodeType::if_) {}

        // for decode
        constexpr If()
            : Expr({}, NodeType::if_) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(expr_type));
            field(sdebugf(cond));
            field(sdebugf(block));
            field(sdebugf(els));
        }
    };

    struct Unary : Expr {
        define_node_type(NodeType::unary);
        std::shared_ptr<Expr> target;
        UnaryOp op;

        constexpr Unary(lexer::Loc l, UnaryOp p)
            : Expr(l, NodeType::unary), op(p) {}

        // for decode
        constexpr Unary()
            : Expr({}, NodeType::unary) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(expr_type));
            auto op = unary_op[int(this->op)];
            field(sdebugf(op));
            field(sdebugf(target));
        }
    };

    struct Binary : Expr {
        define_node_type(NodeType::binary);
        std::shared_ptr<Expr> left;
        std::shared_ptr<Expr> right;
        BinaryOp op;

        Binary(lexer::Loc l, std::shared_ptr<Expr>&& left, BinaryOp op)
            : Expr(l, NodeType::binary), left(std::move(left)), op(op) {}

        // for decode
        constexpr Binary()
            : Expr({}, NodeType::binary) {}

        void dump(auto&& field) {
            auto op = bin_op_str(this->op);
            Expr::dump(field);
            field(sdebugf(expr_type));
            field(sdebugf(op));
            field(sdebugf(left));
            field(sdebugf(right));
        }
    };

    struct MemberAccess : Expr {
        define_node_type(NodeType::member_access);
        std::shared_ptr<Expr> target;
        std::string name;

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(expr_type));
            field(sdebugf(target));
            field(sdebugf(name));
        }

        MemberAccess(lexer::Loc l, std::shared_ptr<Expr>&& t, std::string&& n)
            : Expr(l, NodeType::member_access), target(std::move(t)), name(std::move(n)) {}

        // for decode
        constexpr MemberAccess()
            : Expr({}, NodeType::member_access) {}
    };

    struct Cond : Expr {
        define_node_type(NodeType::cond);
        std::shared_ptr<Expr> then;
        std::shared_ptr<Expr> cond;
        lexer::Loc els_loc;
        std::shared_ptr<Expr> els;

        Cond(lexer::Loc l, std::shared_ptr<Expr>&& then)
            : Expr(l, NodeType::cond), then(std::move(then)) {}

        // for decode
        constexpr Cond()
            : Expr({}, NodeType::cond) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(expr_type));
            field(sdebugf(cond));
            field(sdebugf(then));
            field(sdebugf(els_loc));
            field(sdebugf(els));
        }
    };

    // literals
    struct IntLiteral : Literal {
        define_node_type(NodeType::int_literal);
        std::string raw;

        template <class T>
        std::optional<T> parse_as() const {
            T t = 0;
            if (!utils::number::prefix_integer(raw, t)) {
                return std::nullopt;
            }
            return t;
        }

        void dump(auto&& field) {
            Literal::dump(field);
            field(sdebugf(expr_type));
            field(sdebugf(raw));
            auto num = parse_as<std::int64_t>();
            field(sdebugf(num));
        }

        IntLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, NodeType::int_literal), raw(std::move(t)) {}

        // for decode
        constexpr IntLiteral()
            : Literal({}, NodeType::int_literal) {}
    };

    struct StrLiteral : Literal {
        define_node_type(NodeType::str_literal);
        std::string raw;

        StrLiteral(lexer::Loc l, std::string&& t)
            : Literal(l, NodeType::str_literal), raw(std::move(t)) {}

        // for decode
        constexpr StrLiteral()
            : Literal({}, NodeType::str_literal) {}
    };

    struct BoolLiteral : Literal {
        define_node_type(NodeType::bool_literal);
        bool value;

        void dump(auto&& field) {
            Literal::dump(field);
            field(sdebugf(expr_type));
            field(sdebugf(value));
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

    struct IntType : Type {
        define_node_type(NodeType::int_type);
        std::string raw;
        size_t bit_size = 0;

        IntType(lexer::Loc l, std::string&& token, size_t bit_size)
            : Type(l, NodeType::int_type), raw(std::move(token)), bit_size(bit_size) {}

        // for decode
        constexpr IntType()
            : Type({}, NodeType::int_type) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(raw));
            field(sdebugf(bit_size));
        }
    };

    struct IntLiteralType : Type {
        define_node_type(NodeType::int_literal_type);
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

        void dump(auto&& field) {
            auto bit_size = get_bit_size();
            auto aligned = get_aligned_bit();
            Type::dump(field);
            field(sdebugf(bit_size));
            field(sdebugf(aligned));
        }

        IntLiteralType(const std::shared_ptr<IntLiteral>& ty)
            : Type(ty->loc, NodeType::int_literal_type), base(ty) {}

        // for decode
        constexpr IntLiteralType()
            : Type({}, NodeType::int_literal_type) {}
    };

    struct IdentType : Type {
        define_node_type(NodeType::ident_type);
        std::string ident;
        define_frame frame;
        std::weak_ptr<Fmt> link_to;
        IdentType(lexer::Loc l, std::string&& token, define_frame&& frame)
            : Type(l, NodeType::ident_type), ident(std::move(token)), frame(std::move(frame)) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(ident));
            auto link = link_to.lock();
            field(sdebugf(link));
        }

        // for decode
        constexpr IdentType()
            : Type({}, NodeType::ident_type) {}
    };

    struct VoidType : Type {
        define_node_type(NodeType::void_type);

        VoidType(lexer::Loc l)
            : Type(l, NodeType::void_type) {}

        // for decode
        constexpr VoidType()
            : Type({}, NodeType::void_type) {}

        void dump(auto&& field) {
            Type::dump(field);
        }
    };

    struct BoolType : Type {
        define_node_type(NodeType::bool_type);

        BoolType(lexer::Loc l)
            : Type(l, NodeType::bool_type) {}

        // for decode
        constexpr BoolType()
            : Type({}, NodeType::bool_type) {}

        void dump(auto&& field) {
            Type::dump(field);
        }
    };

    struct ArrayType : Type {
        define_node_type(NodeType::array_type);
        std::shared_ptr<ast::Expr> length;
        lexer::Loc end_loc;
        std::shared_ptr<ast::Type> base_type;

        ArrayType(lexer::Loc l, std::shared_ptr<ast::Expr>&& len, lexer::Loc end, std::shared_ptr<ast::Type>&& base)
            : Type(l, NodeType::array_type), length(std::move(len)), end_loc(end), base_type(std::move(base)) {}

        // for decode
        constexpr ArrayType()
            : Type({}, NodeType::array_type) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(end_loc));
            field(sdebugf(base_type));
            field(sdebugf(length));
        }
    };

    struct StrLiteralType : Type {
        std::weak_ptr<StrLiteral> lit;

        StrLiteralType(std::shared_ptr<StrLiteral>&& str)
            : Type(str->loc, NodeType::str_literal_type), lit(std::move(str)) {}

        // for decode
        constexpr StrLiteralType()
            : Type({}, NodeType::str_literal_type) {}

        void dump(auto&& field) {
            Type::dump(field);
        }
    };

    void debug_def_frames(Debug& d, const define_frame& f);

    struct Program : Node {
        define_node_type(NodeType::program);
        node_list elements;
        define_frame defs;

        void dump(auto&& field) {
            Node::dump(field);
            field(sdebugf(elements));
            field("defs", [&](auto& buf) { debug_defs(buf, defs); });
            field("all_defs", [&](auto& buf) { debug_def_frames(buf, defs); });
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
        if constexpr (std::is_same_v<T, Node>) {
            return v;
        }
        else if constexpr (std::is_same_v<T, Expr> || std::is_same_v<T, Type> ||
                           std::is_same_v<T, Stmt>) {
            if (v && (int(v->type) & int(T::node_type))) {
                return static_cast<T*>(v);
            }
        }
        else {
            if (v && v->type == T::node_type) {
                return static_cast<T*>(v);
            }
        }
        return nullptr;
    }

    template <class U>
    constexpr auto cast_to(auto&& t) {
        using T = std::decay_t<decltype(t)>;
        if constexpr (utils::helper::is_template_instance_of<T, std::shared_ptr>) {
            using V = typename utils::helper::template_instance_of_t<T, std::shared_ptr>::template param_at<0>;
            return std::static_pointer_cast<V>(std::forward<decltype(t)>(t));
        }
        else {
            return static_cast<T*>(t);
        }
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
