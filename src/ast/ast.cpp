/*license*/
#include "stream.h"
#include <mutex>
#include <vector>
#include "expr_layer.h"

namespace ast {

    std::unique_ptr<Expr> parse_prim(Stream& s) {
        if (auto token = s.consume_token(lexer::Tag::int_literal)) {
            return std::make_unique<IntLiteral>(token->loc, std::move(token->token));
        }
        auto token = s.must_consume_token(lexer::Tag::ident);
        return std::make_unique<Ident>(token.loc, std::move(token.token));
    }

    std::optional<lexer::Token> consume_op(Stream& s, size_t& i, const char* const* const ops) {
        for (i = 0; ops[i]; i++) {
            if (auto t = s.consume_token(ops[i])) {
                return t;
            }
        }
        return std::nullopt;
    }

    std::unique_ptr<Expr> parse_unary(Stream& s) {
        std::vector<std::unique_ptr<Unary>> stack;
        size_t i;
        s.skip_space();
        for (;;) {
            if (auto token = consume_op(s, i, ast::unary_op)) {
                stack.push_back(std::make_unique<Unary>(token->loc, UnaryOp(i)));
                s.skip_white();
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
        std::unique_ptr<Expr> expr;
        size_t depth;
        std::vector<BinOpStack> stack;
        size_t i;
        auto parse_low = [&] {
            expr = parse_unary(s);  // return non-nullptr or throw error
            depth = 0;
        };

        auto stack_is_on_depth = [&] {
            return stack.size() && stack.back().depth == depth;
        };

        auto pop_stack = [&] {
            auto op = std::move(stack.back());
            stack.pop_back();
            return op;
        };

        auto update_stack = [&] {  // returns true if `continue` required
            if (stack_is_on_depth()) {
                auto op = pop_stack();
                if (op.expr->type == ObjectType::binary) {
                    if (depth == ast::bin_assign_layer) {
                        stack.push_back(std::move(op));
                    }
                    else {
                        auto b = static_cast<Binary*>(op.expr.get());
                        b->right = std::move(expr);
                        expr = std::move(op.expr);
                    }
                }
                else if (op.expr->type == ObjectType::cond) {
                    auto cop = static_cast<Cond*>(op.expr.get());
                    if (!cop->cond) {
                        cop->cond = std::move(expr);
                        s.skip_white();
                        auto token = s.must_consume_token("else");
                        cop->els_loc = token.loc;
                        stack.push_back(std::move(op));
                        s.skip_white();
                        parse_low();
                        return true;
                    }
                    else {
                        stack.push_back(std::move(op));
                    }
                }
            }
            return false;
        };

        auto post_update_stack = [&] {
            if (depth == ast::bin_cond_layer) {
                while (stack_is_on_depth()) {
                    auto op = pop_stack();
                    auto cop = static_cast<Cond*>(op.expr.get());
                    if (!cop->then || !cop->cond) {
                        s.report_error("expect cond with `cond` but not; parser bug");
                    }
                    cop->els = std::move(expr);
                    expr = std::move(op.expr);
                }
            }
            else if (depth == ast::bin_assign_layer) {
                while (stack_is_on_depth()) {
                    auto op = pop_stack();
                    auto b = static_cast<Binary*>(op.expr.get());
                    if (!b->left) {
                        s.report_error("expect binary with `left` but not; parser bug");
                    }
                    b->right = std::move(expr);
                    expr = std::move(op.expr);
                }
            }
        };

        parse_low();  // first time

        while (depth < ast::bin_layer_len) {
            if (update_stack()) {
                continue;
            }
            s.skip_space();
            if (depth == ast::bin_cond_layer) {
                if (auto token = s.consume_token("if")) {
                    stack.push_back(BinOpStack{.depth = depth, .expr = std::make_unique<Cond>(token->loc, std::move(expr))});
                    s.skip_white();
                    parse_low();
                    continue;
                }
            }
            else {
                if (auto token = consume_op(s, i, ast::bin_layers[depth])) {
                    s.skip_white();
                    auto b = std::make_unique<Binary>(token->loc, std::move(expr), *ast::bin_op(ast::bin_layers[depth][i]));
                    if (depth == 0) {               // special case, needless to use stack
                        b->right = parse_unary(s);  // return non-nullptr or throw error
                        expr = std::move(b);
                    }
                    else {
                        stack.push_back(BinOpStack{.depth = depth, .expr = std::move(b)});
                        parse_low();
                    }
                    continue;
                }
            }
            post_update_stack();
            depth++;
        }
        return expr;
    }

    std::unique_ptr<Program> parse(Stream& s) {
        auto prog = std::make_unique<Program>();
        s.skip_white();
        while (!s.eos()) {
            auto expr = parse_expr(s);
            prog->program.push_back(std::move(expr));
            s.skip_white();
        }
        return prog;
    }

}  // namespace ast
