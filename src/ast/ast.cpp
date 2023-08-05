/*license*/
#include "stream.h"
#include <mutex>
#include <vector>
#include "expr_layer.h"

namespace ast {

    std::unique_ptr<Expr> parse_prim(Stream& s) {
        if (auto token = s.consume_token(lexer::Tag::int_literal)) {
            auto ilit = std::make_unique<IntLiteral>();
            ilit->loc = token->loc;
            ilit->raw = std::move(token->token);
            return ilit;
        }
        auto token = s.must_consume_token(lexer::Tag::ident);
        auto ident = std::make_unique<Ident>();
        ident->loc = token.loc;
        ident->ident = std::move(token.token);
        return ident;
    }

    std::unique_ptr<Expr> parse_unary(Stream& s) {
        std::vector<std::unique_ptr<Unary>> stack;
        size_t i;
        auto consume_op = [&]() -> std::optional<lexer::Token> {
            for (i = 0; ast::unary_op[i]; i++) {
                if (auto t = s.consume_token(ast::unary_op[i])) {
                    return t;
                }
            }
            return std::nullopt;
        };
        s.skip_tag(lexer::Tag::space);
        for (;;) {
            if (auto token = consume_op()) {
                auto p = std::make_unique<Unary>();
                p->loc = token->loc;
                p->op = UnaryOp(i);
                stack.push_back(std::move(p));
                s.skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::indent);
                continue;
            }
            break;
        }
        auto target = parse_prim(s);  // return non-nullptr or throw error
        while (stack.size()) {
            auto ptr = std::move(stack.back());
            ptr->target = std::move(target);
            target = std::move(ptr);
        }
        return target;
    }

    struct BinOpStack {
        size_t depth = 0;
        std::unique_ptr<Expr> expr;
    };

    std::unique_ptr<Expr> parse_expr(Stream& s) {
        auto expr = parse_unary(s);
        if (!expr) {
            return nullptr;
        }
        size_t depth = 0;
        std::vector<BinOpStack> stack;
        auto skip_white = [&] {
            s.skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::indent);
        };
        size_t i;
        auto consume_op = [&]() -> std::optional<lexer::Token> {
            for (i = 0; ast::bin_layers[depth][i]; i++) {
                if (auto t = s.consume_token(ast::bin_layers[depth][i])) {
                    return t;
                }
            }
            return std::nullopt;
        };
        for (; depth < ast::bin_layer_len;) {
            if (stack.size() && stack.back().depth == depth) {
                auto op = std::move(stack.back());
                stack.pop_back();
                if (op.expr->type == ObjectType::binary) {
                    if (depth == ast::bin_assign_layer) {
                        stack.push_back(std::move(op));
                    }
                    else {
                        auto b = static_cast<Binary*>(op.expr.get());
                        b->right = std::move(expr);
                        /* b->expr_type = decide_binary_type(consumer, errs, b);
                        if (!b->expr_type) {
                            return nullptr;
                        }*/
                        expr = std::move(op.expr);
                    }
                }
                else if (op.expr->type == ObjectType::cond) {
                    auto cop = static_cast<Cond*>(op.expr.get());
                    if (!cop->cond) {
                        cop->cond = std::move(expr);
                        skip_white();
                        auto token = s.must_consume_token("else");
                        cop->els_loc = token.loc;
                        stack.push_back(std::move(op));
                        skip_white();
                        depth = 0;
                        expr = parse_unary(s);
                        continue;
                    }
                    else {
                        stack.push_back(std::move(op));
                    }
                }
            }
            s.skip_tag(lexer::Tag::space);
            if (depth == ast::bin_cond_layer) {
                if (auto token = s.consume_token("if")) {
                    auto cop = std::make_unique<Cond>();
                    cop->then = std::move(expr);
                    cop->loc = token->loc;
                    stack.push_back(BinOpStack{.depth = depth, .expr = std::move(cop)});
                    skip_white();
                    depth = 0;
                    expr = parse_unary(s);
                    if (!expr) {
                        return nullptr;
                    }
                    continue;
                }
            }
            else {
                auto fb = s.fallback();
                if (auto token = consume_op()) {
                    skip_white();
                    if (depth == ast::bin_comma_layer && s.expect_token("}")) {
                        break;  // no more expr exists
                    }
                    fb.cancel();
                    auto b = std::make_unique<Binary>();
                    b->loc = token->loc;
                    b->op = *ast::bin_op(ast::bin_layers[depth][i]);
                    if (depth == 0) {
                        b->left = std::move(expr);
                        b->right = parse_unary(s);
                        if (!b->right) {
                            return nullptr;
                        }
                        /* b->expr_type = decide_binary_type(consumer, errs, b.get());
                        if (!b->expr_type) {
                            return nullptr;
                        }*/
                        expr = std::move(b);
                    }
                    else {
                        b->left = std::move(expr);
                        stack.push_back(BinOpStack{.depth = depth, .expr = std::move(b)});
                        depth = 0;
                        expr = parse_unary(s);
                        if (!expr) {
                            return nullptr;
                        }
                    }
                    continue;
                }
            }
            if (depth == ast::bin_cond_layer) {
                while (stack.size() &&
                       stack.back().depth == ast::bin_cond_layer) {
                    auto op = std::move(stack.back());
                    stack.pop_back();
                    auto cop = static_cast<Cond*>(op.expr.get());
                    if (!cop->then || !cop->cond) {
                        s.report_error("expect cond with `cond` but not; parser bug");
                    }
                    cop->els = std::move(expr);
                    expr = std::move(op.expr);
                }
            }
            else if (depth == ast::bin_assign_layer) {
                while (stack.size() &&
                       stack.back().depth == ast::bin_assign_layer) {
                    auto op = std::move(stack.back());
                    stack.pop_back();
                    auto b = static_cast<Binary*>(op.expr.get());
                    if (!b->left) {
                        s.report_error("expect binary with `left` but not; parser bug");
                    }
                    b->right = std::move(expr);
                    /*b->expr_type = decide_binary_type(consumer, errs, b);
                    if (!b->expr_type) {
                        return nullptr;
                    }*/
                    expr = std::move(op.expr);
                }
            }
            depth++;
        }
        return expr;
    }

    std::unique_ptr<Program> parse(Stream& s) {
        auto prog = std::make_unique<Program>();
        while (!s.eos()) {
            s.skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::indent);
            auto expr = parse_expr(s);
            prog->program.push_back(std::move(expr));
        }
        return prog;
    }

}  // namespace ast
