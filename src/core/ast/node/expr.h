/*license*/
#pragma once
#include "base.h"
#include "../expr_layer.h"
#include <vector>
#include <string_view>

namespace brgen::ast {

    // exprs
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

    // this node is used for condition expression
    // to make it immutable
    // by type analysis method, expression is maybe replaced
    // but condition expression is shared with other nodes
    // so we need to make it immutable
    struct Identity : Expr {
        define_node_type(NodeType::identity);
        std::shared_ptr<Expr> expr;
        Identity(std::shared_ptr<Expr>&& e)
            : Expr(e->loc, NodeType::identity), expr(std::move(e)) {}

        // for decode
        Identity()
            : Expr({}, NodeType::identity) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(expr);
        }
    };

    struct StructUnionType;
    struct If : Expr {
        define_node_type(NodeType::if_);
        std::shared_ptr<StructUnionType> struct_union_type;
        scope_ptr cond_scope;
        std::shared_ptr<Identity> cond;
        std::shared_ptr<IndentBlock> then;
        std::shared_ptr<Node> els;

        constexpr If(lexer::Loc l)
            : Expr(l, NodeType::if_) {}

        // for decode
        constexpr If()
            : Expr({}, NodeType::if_) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(struct_union_type);
            sdebugf(cond_scope);
            sdebugf(cond);
            sdebugf(then);
            sdebugf(els);
        }
    };

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

    // .. or ..=
    constexpr bool is_any_range(auto&& e) {
        auto q = static_cast<ast::Node*>(std::to_address(e));
        if (q && q->node_type == NodeType::range) {
            auto p = static_cast<ast::Range*>(q);
            return !p->start && !p->end;
        }
        if (q && q->node_type == NodeType::identity) {
            auto p = static_cast<ast::Identity*>(q);
            return is_any_range(p->expr);
        }
        if (q && q->node_type == NodeType::paren) {
            auto p = static_cast<ast::Paren*>(q);
            return is_any_range(p->expr);
        }
        return false;
    }

    struct MemberAccess : Expr {
        define_node_type(NodeType::member_access);
        std::shared_ptr<Expr> target;
        std::shared_ptr<Ident> member;
        std::weak_ptr<Ident> base;

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

    struct Match;

    struct MatchBranch : Stmt {
        define_node_type(NodeType::match_branch);
        std::weak_ptr<Match> belong;
        std::shared_ptr<Identity> cond;
        lexer::Loc sym_loc;
        std::shared_ptr<Node> then;
        // Comment or CommentGroup
        std::shared_ptr<Node> comment;

        MatchBranch(lexer::Loc l)
            : Stmt(l, NodeType::match_branch) {}
        // for decode
        constexpr MatchBranch()
            : Stmt({}, NodeType::match_branch) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(belong);
            sdebugf(cond);
            sdebugf(sym_loc);
            sdebugf(then);
        }
    };

    struct Match : Expr {
        define_node_type(NodeType::match);
        std::shared_ptr<StructUnionType> struct_union_type;
        scope_ptr cond_scope;
        std::shared_ptr<Identity> cond;
        // MatchBranch
        std::vector<std::shared_ptr<MatchBranch>> branch;

        Match(lexer::Loc l)
            : Expr(l, NodeType::match) {}
        // for decode
        Match()
            : Expr({}, NodeType::match) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(struct_union_type);
            sdebugf(cond_scope);
            sdebugf(cond);
            sdebugf(branch);
        }
    };

    // bad expr, for error tolerant
    // this is error tolerant support node
    struct BadExpr : Expr {
        define_node_type(NodeType::bad_expr);
        std::string content;
        std::shared_ptr<Expr> bad_expr;

        BadExpr(lexer::Loc l, std::string&& c)
            : Expr(l, NodeType::bad_expr), content(std::move(c)) {}

        // for decode
        BadExpr()
            : Expr({}, NodeType::bad_expr) {}

        void dump(auto&& field_) {
            Expr::dump(field_);
            sdebugf(content);
            sdebugf(bad_expr);
        }
    };

}  // namespace brgen::ast
