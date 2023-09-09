/*license*/
#pragma once
#include "stream.h"
#include "node/scope.h"

namespace brgen::ast {

    struct ScopeStack {
       private:
        std::shared_ptr<Scope> root;
        std::shared_ptr<Scope> current;

        void maybe_init() {
            if (!root) {
                root = std::make_shared<Scope>();
                current = root;
            }
            if (current->branch && !current->next) {
                current->next = std::make_shared<Scope>();
                current->next->prev = current;
                current = current->next;
            }
        }

       public:
        void enter_branch() {
            maybe_init();
            current->branch = std::make_shared<Scope>();
            current->branch->prev = current;
            current = current->branch;
        }

        void leave_branch() {
            current = current->prev.lock();
        }

        std::shared_ptr<Scope> current_scope() {
            maybe_init();
            return current;
        }
    };

    struct ParserState {
       private:
        Stream* s = nullptr;
        size_t indent = 0;
        ScopeStack stack;
        std::shared_ptr<Format> current_fmt_;

       public:
        auto new_indent(size_t new_, std::shared_ptr<Scope>* frame) {
            if (indent >= new_) {
                s->report_error("expect larger indent but not");
            }
            auto old = std::exchange(indent, std::move(new_));
            stack.enter_branch();
            *frame = stack.current_scope();
            return utils::helper::defer([=, this] {
                indent = std::move(old);
                stack.leave_branch();
            });
        }

        auto enter_format(const std::shared_ptr<Format>& f) {
            f->belong = current_fmt_;
            current_fmt_ = f;
            return utils::helper::defer([this] {
                current_fmt_ = current_fmt_->belong.lock();
            });
        }

        std::shared_ptr<Format> current_format() {
            return current_fmt_;
        }

        size_t current_indent() {
            return indent;
        }

        scope_ptr reset_stack() {
            stack = {};
            return stack.current_scope();
        }

        scope_ptr current_scope() {
            return stack.current_scope();
        }
    };

    struct Parser {
        Stream& s;
        ParserState state;

        std::shared_ptr<Program> parse() {
            auto prog = std::make_shared<Program>();
            prog->global_scope = state.reset_stack();
            s.skip_line();
            while (!s.eos()) {
                auto expr = parse_statement();
                prog->elements.push_back(std::move(expr));
                s.skip_white();
            }
            return prog;
        }

       private:
        friend struct ParserTest;

        // :\\r\\n
        void must_consume_indent_sign() {
            s.skip_white();
            s.must_consume_token(":");
            s.skip_space();
            s.must_consume_token(lexer::Tag::line);
        }

        std::shared_ptr<IndentScope> parse_indent_block() {
            // Consume the initial indent sign
            must_consume_indent_sign();

            // Get the base indent token
            auto base = s.must_consume_token(lexer::Tag::indent);

            // Create a shared pointer for the IndentScope
            auto scope = std::make_shared<IndentScope>(base.loc);

            // Create a new context for the current indent level
            auto current_indent = base.token.size();
            auto c = state.new_indent(current_indent, &scope->scope);

            // Parse and add the first element
            scope->elements.push_back(parse_statement());

            // Parse and add subsequent elements with the same indent level
            while (auto indent = s.peek_token(lexer::Tag::indent)) {
                if (indent->token.size() < current_indent) {
                    break;
                }
                s.must_consume_token(lexer::Tag::indent);
                scope->elements.push_back(parse_statement());
            }

            return scope;
        }

        std::shared_ptr<If> parse_if() {
            auto token = s.must_consume_token("if");
            s.skip_white();
            auto if_ = std::make_shared<If>(token.loc);

            // 解析して if の条件式とブロックを設定
            if_->cond = parse_expr();
            if_->block = parse_indent_block();

            auto cur_indent = state.current_indent();

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
                elif->cond = parse_expr();
                elif->block = parse_indent_block();
                auto next_if = elif.get();
                current_if->els = std::move(elif);
                current_if = next_if;
                if (detect_end()) {
                    return if_;
                }
            }

            // else ブロックの解析
            if (s.consume_token("else")) {
                current_if->els = parse_indent_block();
                if (cur_indent && !detect_end()) {
                    s.report_error("expect less indent but not");
                }
            }

            return if_;
        }

        std::shared_ptr<Ident> parse_ident_no_frame() {
            auto token = s.must_consume_token(lexer::Tag::ident);
            return std::make_shared<Ident>(token.loc, std::move(token.token));
        }

        std::shared_ptr<Ident> parse_ident() {
            auto ident = parse_ident_no_frame();
            auto frame = state.current_scope();
            frame->push(ident);
            ident->scope = std::move(frame);
            return ident;
        }

        std::shared_ptr<Paren> parse_paren() {
            auto token = s.must_consume_token("(");
            auto paren = std::make_shared<Paren>(token.loc);
            s.skip_white();
            paren->expr = parse_expr();
            s.skip_white();
            token = s.must_consume_token(")");
            paren->end_loc = token.loc;
            return paren;
        }

        std::shared_ptr<Expr> parse_prim() {
            if (auto token = s.consume_token(lexer::Tag::int_literal)) {
                return std::make_shared<IntLiteral>(token->loc, std::move(token->token));
            }
            if (auto b = s.consume_token(lexer::Tag::bool_literal)) {
                return std::make_shared<BoolLiteral>(b->loc, b->token == "true");
            }
            if (s.expect_token("(")) {
                return parse_paren();
            }
            if (s.expect_token("if")) {
                return parse_if();
            }
            return parse_ident();
        }

        std::shared_ptr<Call> parse_call(std::shared_ptr<Expr>& p) {
            auto token = s.must_consume_token("(");
            auto call = std::make_shared<Call>(token.loc, std::move(p));
            s.skip_white();
            if (!s.expect_token(")")) {
                call->arguments = parse_expr();
            }
            token = s.must_consume_token(")");
            call->end_loc = token.loc;
            return call;
        }

        std::shared_ptr<MemberAccess> parse_access(std::shared_ptr<Expr>& p) {
            auto token = s.must_consume_token(".");
            s.skip_white();
            auto ident = s.must_consume_token(lexer::Tag::ident);
            return std::make_shared<MemberAccess>(token.loc, std::move(p), std::move(ident.token));
        }

        std::shared_ptr<Expr> parse_post(Stream& s) {
            auto p = parse_prim();
            for (;;) {
                s.skip_space();
                if (s.expect_token("(")) {
                    p = parse_call(p);
                    continue;
                }
                if (s.expect_token(".")) {
                    p = parse_access(p);
                    continue;
                }
                break;
            }
            return p;
        }

        std::optional<lexer::Token> consume_op(size_t& i, const char* const* const ops) {
            for (i = 0; ops[i]; i++) {
                if (auto t = s.consume_token(ops[i])) {
                    return t;
                }
            }
            return std::nullopt;
        }

        std::shared_ptr<Expr> parse_unary() {
            std::vector<std::shared_ptr<Unary>> stack;
            size_t i;
            s.skip_space();
            for (;;) {
                if (auto token = consume_op(i, ast::unary_op)) {
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

        void check_assignment(ast::Binary* l) {
            if (l->op == ast::BinaryOp::assign ||
                l->op == ast::BinaryOp::typed_assign ||
                l->op == ast::BinaryOp::const_assign) {
                if (l->left->type != ast::NodeType::ident) {
                    s.report_error(l->left->loc, "left of =,:=,::= must be ident");
                }
            }
        }

        struct BinOpStack {
            size_t depth = 0;
            std::shared_ptr<Expr> expr;
        };

        std::shared_ptr<Expr> parse_expr() {
            std::shared_ptr<Expr> expr;
            size_t depth;
            std::vector<BinOpStack> stack;
            size_t i;
            auto parse_low = [&] {
                expr = parse_unary();  // return non-nullptr or throw error
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
                    if (op.expr->type == NodeType::binary) {
                        if (depth == ast::bin_assign_layer) {
                            stack.push_back(std::move(op));
                        }
                        else {
                            auto b = static_cast<Binary*>(op.expr.get());
                            b->right = std::move(expr);
                            expr = std::move(op.expr);
                        }
                    }
                    else if (op.expr->type == NodeType::cond) {
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
                        check_assignment(b);
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
                    if (auto token = consume_op(i, ast::bin_layers[depth])) {
                        if (depth == ast::bin_compare_layer) {
                            if (auto bin = as<Binary>(expr); bin && is_compare_op(bin->op)) {
                                // this is like `a == b == c` but this language not support it
                                s.report_error(token->loc, "unexpected `", token->token, "`");
                            }
                        }
                        s.skip_white();
                        auto b = std::make_shared<Binary>(token->loc, std::move(expr), *ast::bin_op(ast::bin_layers[depth][i]));
                        if (depth == 0) {              // special case, needless to use stack
                            b->right = parse_unary();  // return non-nullptr or throw error
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

        std::shared_ptr<For> parse_for() {
            auto token = s.must_consume_token("for");
            auto for_ = std::make_shared<For>(token.loc);
            for_->block = parse_indent_block();
            return for_;
        }

        std::optional<size_t> is_int_type(std::string_view str) {
            if (str.starts_with("ub") || str.starts_with("ul") ||
                str.starts_with("sb") || str.starts_with("sl")) {
                str = str.substr(2);
            }
            else if (str.starts_with("u") || str.starts_with("s")) {
                str = str.substr(1);
            }
            else {
                return std::nullopt;
            }
            // Check if the string starts with 'u' or 'b' and has a valid unsigned integer
            size_t value = 0;
            if (!utils::number::parse_integer(str.substr(1), value)) {
                return std::nullopt;
            }
            if (value == 0) {  // u0 is not valid
                return std::nullopt;
            }
            return value;
        }

        std::shared_ptr<Type> parse_type() {
            if (auto arr_begin = s.consume_token("[")) {
                s.skip_white();
                auto expr = parse_expr();
                s.skip_white();
                auto end_tok = s.must_consume_token("]");
                s.skip_space();
                auto base_type = parse_type();
                return std::make_shared<ArrayType>(arr_begin->loc, std::move(expr), end_tok.loc, std::move(base_type));
            }

            auto ident = s.must_consume_token(lexer::Tag::ident);

            if (auto bit_size = is_int_type(ident.token)) {
                return std::make_shared<IntType>(ident.loc, std::move(ident.token), *bit_size);
            }

            auto type = std::make_shared<IdentType>(ident.loc, std::move(ident.token), state.current_scope());

            return type;
        }

        // may returns expr if not field
        std::shared_ptr<Node> parse_field(const std::shared_ptr<Expr>& expr) {
            lexer::Token token;
            std::shared_ptr<Ident> ident;
            if (expr) {
                if (expr->type != NodeType::ident) {
                    return expr;
                }
                s.skip_space();
                auto tmp = s.consume_token(":");
                if (!tmp) {
                    return expr;
                }
                ident = cast_to<Ident>(expr);
            }
            else {
                token = s.must_consume_token(":");
            }

            auto field = std::make_shared<Field>(ident ? ident->loc : token.loc);
            field->colon_loc = token.loc;

            if (ident) {
                ident->usage = IdentUsage::define_const;
            }

            field->ident = std::move(ident);
            s.skip_space();

            field->field_type = parse_type();

            if (field->ident) {
                field->ident->expr_type = field->field_type;
                field->belong = state.current_format();
            }

            if (s.consume_token("(")) {
                s.skip_white();

                if (!s.expect_token(")")) {
                    field->arguments = parse_expr();
                    s.skip_white();
                }

                s.must_consume_token(")");
            }

            state.current_scope()->push(field);

            return field;
        }

        std::shared_ptr<Format> parse_format() {
            auto token = s.must_consume_token("format");
            auto fmt = std::make_shared<Format>(token.loc);
            s.skip_space();

            auto ident = s.must_consume_token(lexer::Tag::ident);
            fmt->ident = ident.token;
            {
                auto scope = state.enter_format(fmt);
                fmt->scope = parse_indent_block();
            }

            state.current_scope()->push(fmt);

            return fmt;
        }

        std::shared_ptr<Node> parse_statement() {
            if (s.expect_token("for")) {
                return parse_for();
            }

            if (s.expect_token("format")) {
                return parse_format();
            }

            std::shared_ptr<Node> node;
            if (s.expect_token(":")) {
                node = parse_field(nullptr);
            }
            else {
                auto expr = parse_expr();
                node = parse_field(expr);
            }

            s.skip_space();

            if (!s.eos() && s.expect_token(lexer::Tag::line)) {
                s.must_consume_token(lexer::Tag::line);
                s.skip_line();
            }

            return node;
        }
    };
}  // namespace brgen::ast
