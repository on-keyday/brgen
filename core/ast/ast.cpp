/*license*/
#include "stream.h"
#include <mutex>
#include <vector>
#include "expr_layer.h"

namespace ast {

    void Definitions::add_fmt(std::string& name, const std::shared_ptr<Fmt>& f) {
        fmts[name].push_back(f);
        order.push_back(f);
    }

    void Definitions::add_ident(std::string& name, const std::shared_ptr<Ident>& f) {
        idents[name].push_back(f);
        order.push_back(f);
    }

    void Definitions::add_field(const std::shared_ptr<Field>& f) {
        fields.push_back(f);
        order.push_back(f);
    }

    std::shared_ptr<Object> parse_one(Stream& s);
    std::shared_ptr<Expr> parse_expr(Stream& s);
    // :\\r\\n
    void must_consume_indent_sign(Stream& s) {
        s.skip_white();
        s.must_consume_token(":");
        s.skip_space();
        s.must_consume_token(lexer::Tag::line);
    }

    std::shared_ptr<IndentScope> parse_indent_block(Stream& s) {
        // Consume the initial indent sign
        must_consume_indent_sign(s);

        // Get the base indent token
        auto base = s.must_consume_token(lexer::Tag::indent);

        // Create a shared pointer for the IndentScope
        auto scope = std::make_shared<IndentScope>(base.loc);

        // Create a new context for the current indent level
        auto current_indent = base.token.size();
        auto c = s.context()->new_indent(current_indent, scope->defs);

        // Parse and add the first element
        scope->elements.push_back(parse_one(s));

        // Parse and add subsequent elements with the same indent level
        while (auto indent = s.peek_token(lexer::Tag::indent)) {
            if (indent->token.size() < current_indent) {
                break;
            }
            s.must_consume_token(lexer::Tag::indent);
            scope->elements.push_back(parse_one(s));
        }

        return scope;
    }

    std::shared_ptr<If> parse_if(Stream& s) {
        auto token = s.must_consume_token("if");
        s.skip_white();
        auto if_ = std::make_shared<If>(token.loc);

        // 解析して if の条件式とブロックを設定
        if_->cond = parse_expr(s);
        if_->block = parse_indent_block(s);

        auto cur_indent = s.context()->current_indent();

        auto detect_end = [&] {
            if (cur_indent) {
                auto indent_token = s.peek_token(lexer::Tag::indent);
                if (!indent_token || indent_token->token.size() != cur_indent) {
                    return true;  // 次のインデントが現在のインデントと異なれば終了
                }
                s.must_consume_token(lexer::Tag::indent);
            }
            return false;
        };

        // elif ブロックの解析
        If* current_if = if_.get();
        while (auto tok = s.consume_token("elif")) {
            auto elif = std::make_shared<If>(tok->loc);
            s.skip_white();
            elif->cond = parse_expr(s);
            elif->block = parse_indent_block(s);
            auto next_if = elif.get();
            current_if->els = std::move(elif);
            current_if = next_if;
            if (detect_end()) {
                return if_;
            }
        }

        // else ブロックの解析
        if (s.consume_token("else")) {
            current_if->els = parse_indent_block(s);
            if (cur_indent && !detect_end()) {
                s.report_error("expect less indent but not");
            }
        }

        return if_;
    }

    std::shared_ptr<Ident> parse_ident(Stream& s) {
        auto token = s.must_consume_token(lexer::Tag::ident);
        auto ident = std::make_shared<Ident>(token.loc, std::move(token.token));
        s.context()->current_definitions()->current.add_ident(ident->ident, ident);
        return ident;
    }

    std::shared_ptr<Expr> parse_prim(Stream& s) {
        if (auto token = s.consume_token(lexer::Tag::int_literal)) {
            return std::make_shared<IntLiteral>(token->loc, std::move(token->token));
        }
        if (s.expect_token("if")) {
            return parse_if(s);
        }
        return parse_ident(s);
    }

    std::shared_ptr<Call> parse_call(std::shared_ptr<Expr>& p, Stream& s) {
        auto token = s.must_consume_token("(");
        auto call = std::make_shared<Call>(token.loc, std::move(p));
        s.skip_white();
        if (!s.expect_token(")")) {
            call->arguments = parse_expr(s);
        }
        token = s.must_consume_token(")");
        call->end_loc = token.loc;
        return call;
    }

    std::shared_ptr<MemberAccess> parse_access(std::shared_ptr<Expr>& p, Stream& s) {
        auto token = s.must_consume_token(".");
        s.skip_white();
        auto ident = s.must_consume_token(lexer::Tag::ident);
        return std::make_shared<MemberAccess>(token.loc, std::move(p), std::move(ident.token));
    }

    std::shared_ptr<Expr> parse_post(Stream& s) {
        auto p = parse_prim(s);
        for (;;) {
            s.skip_space();
            if (s.expect_token("(")) {
                p = parse_call(p, s);
                continue;
            }
            if (s.expect_token(".")) {
                p = parse_access(p, s);
                continue;
            }
            break;
        }
        return p;
    }

    std::optional<lexer::Token> consume_op(Stream& s, size_t& i, const char* const* const ops) {
        for (i = 0; ops[i]; i++) {
            if (auto t = s.consume_token(ops[i])) {
                return t;
            }
        }
        return std::nullopt;
    }

    std::shared_ptr<Expr> parse_unary(Stream& s) {
        std::vector<std::shared_ptr<Unary>> stack;
        size_t i;
        s.skip_space();
        for (;;) {
            if (auto token = consume_op(s, i, ast::unary_op)) {
                stack.push_back(std::make_shared<Unary>(token->loc, UnaryOp(i)));
                s.skip_white();
                continue;
            }
            break;
        }
        auto target = parse_post(s);  // return non-nullptr or throw error
        while (stack.size()) {
            auto ptr = std::move(stack.back());
            stack.pop_back();
            ptr->target = std::move(target);
            target = std::move(ptr);
        }
        return target;
    }

    void check_assignment(Stream& s, ast::Binary* l) {
        if (l->op == ast::BinaryOp::assign) {
            if (l->left->type != ast::ObjectType::ident) {
                s.report_error(l->left->loc, "left of = must be ident");
            }
        }
    }

    struct BinOpStack {
        size_t depth = 0;
        std::shared_ptr<Expr> expr;
    };

    std::shared_ptr<Expr> parse_expr(Stream& s) {
        std::shared_ptr<Expr> expr;
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
                    check_assignment(s, b);
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
                    stack.push_back(BinOpStack{.depth = depth, .expr = std::make_shared<Cond>(token->loc, std::move(expr))});
                    s.skip_white();
                    parse_low();
                    continue;
                }
            }
            else {
                if (auto token = consume_op(s, i, ast::bin_layers[depth])) {
                    s.skip_white();
                    auto b = std::make_shared<Binary>(token->loc, std::move(expr), *ast::bin_op(ast::bin_layers[depth][i]));
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

    std::shared_ptr<For> parse_for(Stream& s) {
        auto token = s.must_consume_token("for");
        auto for_ = std::make_shared<For>(token.loc);
        for_->block = parse_indent_block(s);
        return for_;
    }

    std::optional<size_t> is_int_type(std::string_view str) {
        // Check if the string starts with 'u' or 'b' and has a valid unsigned integer
        if (str.size() > 1 && (str[0] == 'u' || str[0] == 'b')) {
            size_t value = 0;
            if (!utils::number::parse_integer(str.substr(1), value)) {
                return std::nullopt;
            }
            if (value == 0) {  // u0 is not valid
                return std::nullopt;
            }
            return value;
        }
        return std::nullopt;
    }

    std::shared_ptr<Type> parse_type(Stream& s) {
        auto ident = s.must_consume_token(lexer::Tag::ident);

        if (auto bit_size = is_int_type(ident.token)) {
            return std::make_shared<IntegerType>(ident.loc, std::move(ident.token), *bit_size);
        }

        auto type = std::make_shared<IdentType>(ident.loc, std::move(ident.token), s.context()->current_definitions());

        if (s.consume_token("(")) {
            s.skip_white();

            if (!s.expect_token(")")) {
                type->arguments = parse_expr(s);
                s.skip_white();
            }

            s.must_consume_token(")");
        }

        return type;
    }

    std::optional<std::shared_ptr<Field>> parse_field(Stream& s) {
        auto f = s.fallback();
        std::shared_ptr<Ident> ident;
        if (s.expect_token(lexer::Tag::ident)) {
            ident = parse_ident(s);
        }

        if (ident) {
            s.skip_space();
        }

        auto token = s.consume_token(":");

        if (!token) {
            return std::nullopt;
        }
        f.cancel();

        auto field = std::make_shared<Field>(ident ? ident->loc : token->loc);
        field->colon_loc = token->loc;

        field->ident = std::move(ident);

        field->field_type = parse_type(s);

        s.context()->current_definitions()->current.add_field(field);

        return field;
    }

    std::shared_ptr<Fmt> parse_fmt(Stream& s) {
        auto token = s.must_consume_token("fmt");
        auto fmt = std::make_shared<Fmt>(token.loc);
        s.skip_space();

        auto ident = s.must_consume_token(lexer::Tag::ident);
        fmt->ident = ident.token;
        fmt->scope = parse_indent_block(s);

        s.context()->current_definitions()->current.add_fmt(fmt->ident, fmt);

        return fmt;
    }

    std::shared_ptr<Object> parse_one(Stream& s) {
        if (s.expect_token("for")) {
            return parse_for(s);
        }

        if (s.expect_token("fmt")) {
            return parse_fmt(s);
        }

        std::shared_ptr<Object> obj;

        if (auto field = parse_field(s)) {
            obj = std::move(*field);
        }
        else {
            obj = parse_expr(s);
        }

        s.skip_space();

        if (!s.eos() && s.expect_token(lexer::Tag::line)) {
            s.must_consume_token(lexer::Tag::line);
            s.skip_line();
        }

        return obj;
    }

    std::shared_ptr<Program> parse(Stream& s) {
        auto prog = std::make_shared<Program>();
        prog->defs = s.context()->reset_stack();
        s.skip_line();
        while (!s.eos()) {
            auto expr = parse_one(s);
            prog->elements.push_back(std::move(expr));
            s.skip_white();
        }
        return prog;
    }

}  // namespace ast
