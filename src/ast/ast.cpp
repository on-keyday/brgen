/*license*/
#include "stream.h"
#include <mutex>
#include <vector>
#include "expr_layer.h"

namespace ast {

    struct BinOpStack {
        size_t depth = 0;
        std::unique_ptr<Expr> expr;
    };

    std::unique_ptr<Expr> parse_unary(Stream& s) {}

    std::unique_ptr<Expr> parse_expr(Stream& s) {
        auto expr = parse_unary(s);
        if (!expr) {
            return nullptr;
        }
        size_t depth = 0;
        std::vector<BinOpStack> stacks;
        for (; depth < ast::bin_layer_len;) {
            if (stacks.size() && stacks.back().depth == depth) {
                auto op = std::move(stacks.back());
                stacks.pop_back();
                if (op.expr->type == ObjectType::binary) {
                    if (depth == ast::bin_assign_layer) {
                        stacks.push_back(std::move(op));
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
                else if (op.expr->type == ObjectType::cond_op) {
                    auto cop = static_cast<CondOp*>(op.expr.get());
                    if (!cop->then_) {
                        cop->then_ = std::move(expr);
                        if (!consumer.expect_token(":")) {
                            errs.push(Error{.err = "expect : but found " + consumer.token(), .pos = consumer.pos()});
                            return nullptr;
                        }
                        cop->els_pos = consumer.pos();
                        stacks.push_back(std::move(op));
                        consumer.consume_and_skip();
                        depth = 0;
                        expr = parse_cast(consumer, errs);
                        continue;
                    }
                    else {
                        stacks.push_back(std::move(op));
                    }
                }
            }
            s.skip_tag(lexer::Tag::space);
            auto skip_white = [&] {
                s.skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::indent);
            };
            if (depth == ast::bin_cond_layer) {
                if (auto token = s.consume_token("if")) {
                    auto cop = std::make_unique<CondOp>();
                    cop->cond = std::move(expr);
                    cop->pos = token->loc;
                    stacks.push_back(BinOpStack{.depth = depth, .expr = std::move(cop)});
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
                size_t i;
                auto consume_op = [&] {
                    for (i = 0; ast::bin_layers[depth][i]; i++) {
                        if (auto t = s.consume_token(ast::bin_layers[depth][i])) {
                            return t;
                        }
                    }
                    return std::nullopt;
                };
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
                        stacks.push_back(BinOpStack{.depth = depth, .expr = std::move(b)});
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
                while (stacks.size() &&
                       stacks.back().depth == ast::bin_cond_layer) {
                    auto op = std::move(stacks.back());
                    stacks.pop_back();
                    auto cop = static_cast<Cond*>(op.expr.get());
                    if (!cop->then) {
                        errs.push(Error{.err = "unexpected state, parser errpr", .pos = consumer.pos()});
                        return nullptr;
                    }
                    cop->else_ = std::move(expr);
                    expr = std::move(op.expr);
                }
            }
            else if (depth == ast::bin_assign_layer) {
                while (stacks.size() &&
                       stacks.back().depth == ast::bin_assign_layer) {
                    auto op = std::move(stacks.back());
                    stacks.pop_back();
                    auto b = static_cast<Binary*>(op.expr.get());
                    b->right = std::move(expr);
                    b->expr_type = decide_binary_type(consumer, errs, b);
                    if (!b->expr_type) {
                        return nullptr;
                    }
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
            s.skip_tag(lexer::Tag::space, lexer::Tag::line);
            auto token = s.must_consume_token(lexer::Tag::int_literal);
            auto ilit = std::make_unique<IntLiteral>();
            ilit->loc = token.loc;
            ilit->raw = token.token;
            prog->program.push_back(std::move(ilit));
        }
        return prog;
    }

}  // namespace ast
