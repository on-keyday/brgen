/*license*/
#pragma once
#include "base.h"
#include "../expr_layer.h"
#include <vector>
#include <string_view>

namespace brgen::ast {

    // exprs
    /*
        enum class IdentUsage {
            unknown,
            reference,
            define_variable,
            define_const,
            define_field,
            define_format,
            define_state,
            define_enum,
            define_enum_member,
            define_fn,
            define_cast_fn,
            define_arg,
            reference_type,
            reference_member,
            maybe_type,
            reference_builtin_fn,
        };

        constexpr const char* ident_usage_str[]{
            "unknown",
            "reference",
            "define_variable",
            "define_const",
            "define_field",
            "define_format",
            "define_state",
            "define_enum",
            "define_enum_member",
            "define_fn",
            "define_cast_fn",
            "define_arg",
            "reference_type",
            "reference_member",
            "maybe_type",
            "reference_builtin_fn",
            nullptr,
        };

        constexpr auto ident_usage_count = std::size(ident_usage_str) - 1;

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
    */
    struct Ident : Expr {
        define_node_type(NodeType::ident);
        std::string ident;
        scope_ptr scope;
        std::weak_ptr<Node> base;
        IdentUsage usage = IdentUsage::unknown;

        Ident(lexer::Loc l, std::string&& i)
            : Expr(l, NodeType::ident), ident(std::move(i)) {}

        Ident(lexer::Loc l, const std::string& i)
            : Expr(l, NodeType::ident), ident(i) {}

        // for decode
        Ident()
            : Expr({}, NodeType::ident) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(ident);
            sdebugf(usage);
            sdebugf(base);
            sdebugf(scope);
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

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(callee);
            sdebugf_omit(raw_arguments);
            sdebugf(arguments);
            sdebugf(end_loc);
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

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(expr);
            sdebugf(end_loc);
        }
    };

    struct If : Expr {
        define_node_type(NodeType::if_);
        scope_ptr cond_scope;
        std::shared_ptr<Expr> cond;
        std::shared_ptr<IndentBlock> then;
        std::shared_ptr<Node> els;

        constexpr If(lexer::Loc l)
            : Expr(l, NodeType::if_) {}

        // for decode
        constexpr If()
            : Expr({}, NodeType::if_) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(cond_scope);
            sdebugf(cond);
            sdebugf(then);
            sdebugf(els);
        }
    };
    /*
        constexpr void as_json(UnaryOp op, auto&& buf) {
            buf.value(unary_op_str[int(op)]);
        }
    */
    struct Unary : Expr {
        define_node_type(NodeType::unary);
        std::shared_ptr<Expr> expr;
        UnaryOp op;

        constexpr Unary(lexer::Loc l, UnaryOp p)
            : Expr(l, NodeType::unary), op(p) {}

        // for decode
        constexpr Unary()
            : Expr({}, NodeType::unary) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(op);
            sdebugf(expr);
        }
    };

    /*
        constexpr void as_json(BinaryOp op, auto&& buf) {
            buf.value(bin_op_str(op));
        }
    */
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

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(op);
            sdebugf(left);
            sdebugf(right);
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

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(op);
            sdebugf(start);
            sdebugf(end);
        }
    };

    struct MemberAccess : Expr {
        define_node_type(NodeType::member_access);
        std::shared_ptr<Expr> target;
        std::shared_ptr<Ident> member;
        std::weak_ptr<Node> base;

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(target);
            sdebugf(member);
            sdebugf(base);
        }

        MemberAccess(lexer::Loc l, std::shared_ptr<Expr>&& t, std::shared_ptr<Ident>&& n)
            : Expr(l, NodeType::member_access), target(std::move(t)), member(std::move(n)) {}

        // for decode
        MemberAccess()
            : Expr({}, NodeType::member_access) {}
    };

    struct Cond : Expr {
        define_node_type(NodeType::cond);
        std::shared_ptr<Expr> cond;
        std::shared_ptr<Expr> then;
        lexer::Loc els_loc;
        std::shared_ptr<Expr> els;

        Cond(lexer::Loc l, std::shared_ptr<Expr>&& cond)
            : Expr(l, NodeType::cond), cond(std::move(cond)) {}

        // for decode
        constexpr Cond()
            : Expr({}, NodeType::cond) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(cond);
            sdebugf(then);
            sdebugf(els_loc);
            sdebugf(els);
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

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(expr);
            sdebugf(index);
            sdebugf(end_loc);
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

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(cond);
            sdebugf(sym_loc);
            sdebugf(then);
        }
    };

    struct Match : Expr {
        define_node_type(NodeType::match);
        scope_ptr cond_scope;
        std::shared_ptr<Expr> cond;
        // MatchBranch or Comment or CommentGroup
        std::list<std::shared_ptr<Node>> branch;

        Match(lexer::Loc l)
            : Expr(l, NodeType::match) {}
        // for decode
        Match()
            : Expr({}, NodeType::match) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(cond_scope);
            sdebugf(cond);
            sdebugf(branch);
        }
    };

}  // namespace brgen::ast
