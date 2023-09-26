/*license*/
#pragma once
#include "base.h"
#include "../expr_layer.h"
#include <vector>
#include <string_view>

namespace brgen::ast {

    // exprs

    enum class IdentUsage {
        unknown,
        reference,
        define_variable,
        define_const,
        define_field,
        define_format,
        define_fn,
        reference_type,
    };

    constexpr const char* ident_usage_str[]{
        "unknown",
        "reference",
        "define_variable",
        "define_const",
        "define_field",
        "define_format",
        "define_fn",
        "reference_type",
        nullptr,
    };

    constexpr auto ident_usage_count = 8;

    constexpr std::optional<IdentUsage> ident_usage(std::string_view view) {
        for (auto i = 0; ident_usage_str[i]; i++) {
            if (ident_usage_str[i] == view) {
                return IdentUsage(i);
            }
        }
        return std::nullopt;
    }

    constexpr void as_json(IdentUsage usage, auto&& buf) {
        buf.string(ident_usage_str[int(usage)]);
    }

    struct Ident : Expr {
        define_node_type(NodeType::ident);
        std::string ident;
        scope_ptr scope;
        std::weak_ptr<Node> base;
        IdentUsage usage = IdentUsage::unknown;

        Ident(lexer::Loc l, std::string&& i)
            : Expr(l, NodeType::ident), ident(std::move(i)) {}

        // for decode
        Ident()
            : Expr({}, NodeType::ident) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(ident));
            field(sdebugf(usage));
            field(sdebugf(base));
            field(sdebugf(scope));
        }
    };

    struct Call : Expr {
        define_node_type(NodeType::call);
        std::shared_ptr<Expr> callee;
        std::shared_ptr<Expr> raw_arguments;
        std::vector<std::shared_ptr<Expr>> arguments;
        lexer::Loc end_loc;
        Call(lexer::Loc l, std::shared_ptr<Expr>&& callee)
            : Expr(l, NodeType::call), callee(std::move(callee)) {}
        Call()
            : Expr({}, NodeType::call) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(callee));
            field(sdebugf(raw_arguments));
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
            field(sdebugf(expr));
            field(sdebugf(end_loc));
        }
    };

    struct If : Expr {
        define_node_type(NodeType::if_);
        std::shared_ptr<Expr> cond;
        std::shared_ptr<IndentScope> then;
        std::shared_ptr<Node> els;

        constexpr If(lexer::Loc l)
            : Expr(l, NodeType::if_) {}

        // for decode
        constexpr If()
            : Expr({}, NodeType::if_) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(cond));
            field(sdebugf(then));
            field(sdebugf(els));
        }
    };

    constexpr void as_json(UnaryOp op, auto&& buf) {
        buf.value(unary_op_str[int(op)]);
    }

    struct Unary : Expr {
        define_node_type(NodeType::unary);
        std::shared_ptr<Expr> expr;
        UnaryOp op;

        constexpr Unary(lexer::Loc l, UnaryOp p)
            : Expr(l, NodeType::unary), op(p) {}

        // for decode
        constexpr Unary()
            : Expr({}, NodeType::unary) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(op));
            field(sdebugf(expr));
        }
    };

    constexpr void as_json(BinaryOp op, auto&& buf) {
        buf.value(bin_op_str(op));
    }

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
            Expr::dump(field);
            field(sdebugf(op));
            field(sdebugf(left));
            field(sdebugf(right));
        }
    };

    struct Range : Expr {
        define_node_type(NodeType::range);
        std::shared_ptr<Expr> start;
        std::shared_ptr<Expr> end;
        BinaryOp op;

        Range(lexer::Loc l, std::shared_ptr<Expr>&& left, BinaryOp op)
            : Expr(l, NodeType::range), start(std::move(left)), op(op) {}

        // for decode
        constexpr Range()
            : Expr({}, NodeType::range) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(op));
            field(sdebugf(start));
            field(sdebugf(end));
        }
    };

    struct MemberAccess : Expr {
        define_node_type(NodeType::member_access);
        std::shared_ptr<Expr> target;
        std::string member;
        lexer::Loc member_loc;

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(target));
            field(sdebugf(member));
            field(sdebugf(member_loc));
        }

        MemberAccess(lexer::Loc l, std::shared_ptr<Expr>&& t, std::string&& n, lexer::Loc ml)
            : Expr(l, NodeType::member_access), target(std::move(t)), member(std::move(n)), member_loc(ml) {}

        // for decode
        MemberAccess()
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
            field(sdebugf(cond));
            field(sdebugf(then));
            field(sdebugf(els_loc));
            field(sdebugf(els));
        }
    };

    struct Index : Expr {
        define_node_type(NodeType::index);
        std::shared_ptr<Expr> expr;
        std::shared_ptr<Expr> index;
        lexer::Loc end_loc;

        // for decode
        Index(lexer::Loc l, std::shared_ptr<Expr>&& e)
            : Expr(l, NodeType::index), expr(std::move(e)) {}
        // for decode
        constexpr Index()
            : Expr({}, NodeType::index) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(expr));
            field(sdebugf(index));
            field(sdebugf(end_loc));
        }
    };

    struct MatchBranch : Stmt {
        define_node_type(NodeType::match_branch);
        std::shared_ptr<Expr> cond;
        lexer::Loc sym_loc;
        std::shared_ptr<Node> then;

        MatchBranch(lexer::Loc l)
            : Stmt(l, NodeType::match_branch) {}
        // for decode
        constexpr MatchBranch()
            : Stmt({}, NodeType::match_branch) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(cond));
            field(sdebugf(sym_loc));
            field(sdebugf(then));
        }
    };

    struct Match : Expr {
        define_node_type(NodeType::match);
        std::shared_ptr<Expr> cond;
        std::list<std::shared_ptr<MatchBranch>> branch;
        scope_ptr scope;

        Match(lexer::Loc l)
            : Expr(l, NodeType::match) {}
        // for decode
        Match()
            : Expr({}, NodeType::match) {}

        void dump(auto&& field) {
            Expr::dump(field);
            field(sdebugf(cond));
            field(sdebugf(branch));
            field(sdebugf(scope));
        }
    };

}  // namespace brgen::ast
