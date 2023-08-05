/*license*/
#include "stream.h"
#include <mutex>
#include <vector>
#include "expr_layer.h"

namespace ast {

    std::unique_ptr<Object> parse_one(Stream& s);
    std::unique_ptr<Expr> parse_expr(Stream& s);

    std::unique_ptr<Block> parse_indent_block(Stream& s) {
        auto base = s.must_consume_token(lexer::Tag::indent);
        auto c = s.context()->new_indent(base.token.size());
        auto scope = std::make_unique<IndentScope>(base.loc);
        scope->elements.push_back(parse_one(s));
        for (;;) {
            auto indent = s.peek_token(lexer::Tag::indent);
            if (!indent || indent->token.size() < base.token.size()) {
                break;
            }
            s.must_consume_token(lexer::Tag::indent);
            scope->elements.push_back(parse_one(s));
        }
        return scope;
    }

    std::unique_ptr<Expr> parse_if(Stream& s) {
        auto token = s.must_consume_token("if");
        s.skip_white();
        auto if_ = std::make_unique<If>(token.loc);
        if_->cond = parse_expr(s);
        s.skip_white();
        s.must_consume_token(":");
        s.skip_space();
        s.must_consume_token(lexer::Tag::line);
        if_->block = parse_indent_block(s);
        auto cur = s.context()->current_indent();
        auto detect_end = [&] {
            if (cur) {
                auto indent = s.peek_token(lexer::Tag::indent);
                if (!indent || indent->token.size() != cur) {
                    return true;  // nothing to do
                }
                s.must_consume_token(lexer::Tag::indent);
            }
            return false;
        };
        if (detect_end()) {
            return if_;
        }
        If* elifs = if_.get();
        for (auto tok = s.consume_token("elif"); tok; tok = s.consume_token("elif")) {
            auto elif = std::make_unique<If>(tok->loc);
            s.skip_white();
            elif->cond = parse_expr(s);
            s.skip_white();
            s.must_consume_token(":");
            s.skip_space();
            s.must_consume_token(lexer::Tag::line);
            elif->block = parse_indent_block(s);
            auto new_else = elif.get();
            elifs->els = std::move(elif);
            elifs = new_else;
            if (detect_end()) {
                return if_;
            }
        }
        if (!s.consume_token("else")) {
            return if_;  // nothing to do
        }
        s.skip_white();
        s.must_consume_token(":");
        s.skip_space();
        s.must_consume_token(lexer::Tag::line);
        elifs->els = parse_indent_block(s);
        if (cur && !detect_end()) {
            s.report_error("expect less indent but not");
        }
        return if_;
    }

    std::unique_ptr<Expr> parse_prim(Stream& s) {
        if (auto token = s.consume_token(lexer::Tag::int_literal)) {
            return std::make_unique<IntLiteral>(token->loc, std::move(token->token));
        }
        if (s.expect_token("if")) {
            return parse_if(s);
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

    std::unique_ptr<Object> parse_one(Stream& s) {
        auto obj = parse_expr(s);
        s.skip_space();
        if (!s.eos() && !s.expect_token(lexer::Tag::indent)) {
            s.must_consume_token(lexer::Tag::line);
            s.skip_line();
        }
        return obj;
    }

    std::unique_ptr<Program> parse(Stream& s) {
        auto prog = std::make_unique<Program>();
        s.skip_line();
        while (!s.eos()) {
            auto expr = parse_one(s);
            prog->program.push_back(std::move(expr));
            s.skip_white();
        }
        return prog;
    }

}  // namespace ast
