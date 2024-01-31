/*license*/
#pragma once
#include "stream.h"
#include "node/scope.h"

namespace brgen::ast {

    struct ParserState {
        std::shared_ptr<Type> input_type;
        std::shared_ptr<Type> output_type;
        std::shared_ptr<Type> config_type;
        bool collect_comment = false;

       private:
        size_t indent = 0;
        ScopeStack stack;
        std::shared_ptr<Member> current_fmt_;
        std::shared_ptr<StructType> current_struct_;

       public:
        auto cond_scope(scope_ptr& frame, const std::shared_ptr<Node>& scope_owner) {
            auto br = stack.enter_branch();
            frame = stack.current_scope();
            frame->owner = scope_owner;
            return br;
        }

        auto new_indent(Stream& s, size_t new_, scope_ptr& frame, const std::shared_ptr<Node>& scope_owner) {
            if (indent >= new_) {
                s.report_error("expect larger indent but not");
            }
            auto old = std::exchange(indent, std::move(new_));
            return futils::helper::defer([this, old, sc = cond_scope(frame, scope_owner)] {
                indent = std::move(old);
            });
        }

        auto new_indent_no_scope(Stream& s, size_t new_) {
            if (indent >= new_) {
                s.report_error("expect larger indent but not");
            }
            auto old = std::exchange(indent, std::move(new_));
            return futils::helper::defer([=, this] {
                indent = std::move(old);
            });
        }

        auto enter_member(const std::shared_ptr<Member>& f) {
            f->belong = current_fmt_;
            current_fmt_ = f;
            return futils::helper::defer([this] {
                current_fmt_ = current_fmt_->belong.lock();
            });
        }

        auto enter_struct(const std::shared_ptr<StructType>& type) {
            auto tmp = current_struct_;
            current_struct_ = type;
            return futils::helper::defer([this, tmp] {
                current_struct_ = std::move(tmp);
            });
        }

        std::shared_ptr<Member> current_member() {
            return current_fmt_;
        }

        void add_to_struct(const std::shared_ptr<Member>& f) {
            f->belong_struct = current_struct_;
            current_struct_->fields.push_back(f);
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
            prog->loc = s.loc();
            prog->global_scope = state.reset_stack();
            prog->global_scope->owner = prog;
            prog->struct_type = std::make_shared<StructType>(prog->loc);
            prog->struct_type->base = prog;
            auto st = state.enter_struct(prog->struct_type);
            s.skip_line();
            auto collect_comments = [&] {
                // get comments and add to scope
                auto comment = s.get_comments();
                if (comment) {
                    prog->elements.push_back(std::move(comment));
                }
            };
            while (!s.eos()) {
                collect_comments();
                auto expr = parse_statement();
                prog->elements.push_back(std::move(expr));
                s.shrink();
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
            s.skip_line();
        }

        /*
            <indent scope> ::= ":" <line> (<indent> <statement>)+
        */
        std::shared_ptr<IndentBlock> parse_indent_block(const std::shared_ptr<Node>& scope_owner, std::vector<std::shared_ptr<Ident>>* ident = nullptr) {
            // Consume the initial indent sign
            must_consume_indent_sign();

            // Get the base indent token
            auto base = s.must_consume_token(lexer::Tag::indent);

            // Create a shared pointer for the IndentBlock
            auto block = std::make_shared<IndentBlock>(base.loc);

            block->struct_type = std::make_shared<StructType>(base.loc);

            assert(scope_owner != nullptr);
            block->struct_type->base = scope_owner;

            // Create a new context for the current indent level
            auto current_indent = base.token.size();
            auto c = state.new_indent(s, current_indent, block->scope, scope_owner);
            auto ss = state.enter_struct(block->struct_type);

            if (ident) {
                for (auto& i : *ident) {
                    block->scope->push(std::move(i));
                }
            }

            auto collect_comments = [&] {
                // get comments and add to scope
                auto comment = s.get_comments();
                if (comment) {
                    block->elements.push_back(std::move(comment));
                }
            };

            auto parse_a_line = [&] {
                collect_comments();
                bool line_skipped = false;
                while (!line_skipped) {
                    block->elements.push_back(parse_statement(&line_skipped));
                    if (s.peek_token(lexer::Tag::indent) || s.eos()) {
                        break;
                    }
                }
            };

            // Parse and add the first element
            parse_a_line();

            // Parse and add subsequent elements with the same indent level
            while (auto indent = s.peek_token(lexer::Tag::indent)) {
                if (indent->token.size() != current_indent) {
                    break;
                }
                s.must_consume_token(lexer::Tag::indent);
                parse_a_line();
            }

            return block;
        }

        void export_union_field(const std::shared_ptr<Expr>& cond0, std::vector<std::shared_ptr<Expr>>& cond, const std::shared_ptr<StructUnionType>& type) {
            assert(cond.size() == type->structs.size());
            std::map<std::string, std::vector<std::shared_ptr<UnionCandidate>>> m;
            for (size_t i = 0; i < cond.size(); i++) {
                auto& c = cond[i];
                auto& f = type->structs[i];
                for (auto& d : f->fields) {
                    if (!d->ident) {
                        continue;
                    }
                    if (!ast::as<Field>(d)) {
                        continue;
                    }
                    if (auto found = m.find(d->ident->ident); found != m.end()) {
                        for (auto& c1 : found->second) {
                            if (c1->cond.lock() == c) {
                                error(d->loc, "duplicate field name: ", d->ident->ident).error(c->loc, "previous definition is here").report();
                            }
                        }
                    }
                    auto cand = std::make_shared<UnionCandidate>(d->loc);
                    cand->cond = c;
                    cand->field = ast::cast_to<Field>(d);
                    m[d->ident->ident].push_back(std::move(cand));
                }
            }
            std::vector<std::shared_ptr<UnionCandidate>> null_cache;
            auto get_null_cache = [&](size_t i) {
                assert(i < cond.size());
                if (null_cache.size() <= i) {
                    null_cache.resize(i + 1);
                }
                if (!null_cache[i]) {
                    null_cache[i] = std::make_shared<UnionCandidate>(type->loc);
                    null_cache[i]->cond = cond[i];
                }
                return null_cache[i];
            };
            for (auto& [k, v] : m) {
                auto union_type = std::make_shared<UnionType>();
                union_type->cond = cond0;
                union_type->loc = v[0]->loc;
                auto ident = std::make_shared<Ident>(union_type->loc, k);
                ident->usage = IdentUsage::define_field;
                ident->scope = state.current_scope();
                ident->scope->push(ident);
                ident->expr_type = union_type;
                auto field = std::make_shared<Field>(union_type->loc);
                field->ident = ident;
                ident->base = field;
                field->field_type = union_type;
                union_type->base_type = type;
                size_t cand_i = 0;
                for (auto& c : v) {
                    while (c->cond.lock() != cond[cand_i]) {
                        union_type->candidates.push_back(get_null_cache(cand_i));
                        cand_i++;
                    }
                    union_type->candidates.push_back(c);
                    cand_i++;
                }
                type->union_fields.push_back(field);
                state.add_to_struct(std::move(field));
            }
        }

        /*
            <match> ::= "match" <expr>? <match branch>*
            <match branch> ::= <expr> [":" <indent block> | "=>" <statement>]
        */
        std::shared_ptr<Match> parse_match(lexer::Token&& token) {
            // Create a shared pointer for the Match
            auto match = std::make_shared<Match>(token.loc);

            std::shared_ptr<StructUnionType> union_ = std::make_shared<StructUnionType>(match->loc);
            union_->base = match;

            std::vector<std::shared_ptr<Expr>> cond;

            auto cs = state.cond_scope(match->cond_scope, match);

            auto push_union_to_current_struct = [&] {
                auto f = std::make_shared<Field>(match->loc);
                f->field_type = union_;
                f->belong = state.current_member();
                state.add_to_struct(std::move(f));
                cs.execute();
                export_union_field(match->cond, cond, union_);
            };

            s.skip_white();

            if (!s.expect_token(":")) {
                match->cond = parse_expr();
            }

            // Consume the initial indent sign
            must_consume_indent_sign();

            auto stmt_with_struct = [&](lexer::Loc loc, std::shared_ptr<ast::MatchBranch>& br) {
                auto scoped = std::make_shared<ScopedStatement>(loc);
                scoped->struct_type = std::make_shared<StructType>(loc);
                scoped->struct_type->base = br;
                auto s_scope = state.enter_struct(scoped->struct_type);
                auto c_scope = state.cond_scope(scoped->scope, br);
                scoped->statement = parse_statement();
                union_->structs.push_back(scoped->struct_type);
                br->then = std::move(scoped);
            };

            auto parse_match_branch = [&]() -> std::shared_ptr<MatchBranch> {
                auto br = std::make_shared<MatchBranch>();
                br->belong = match;
                br->cond = parse_expr();
                cond.push_back(br->cond);
                br->loc = br->cond->loc;
                s.skip_white();
                auto sym = s.consume_token("=>");
                if (!sym) {
                    auto tok = s.peek_token(":");
                    auto block = parse_indent_block(br);
                    union_->structs.push_back(block->struct_type);
                    br->then = std::move(block);
                    br->sym_loc = tok->loc;
                    return br;
                }
                br->sym_loc = sym->loc;
                s.skip_white();
                stmt_with_struct(sym->loc, br);
                return br;
            };

            // Get the base indent token
            auto base = s.must_consume_token(lexer::Tag::indent);

            // Create a new context for the current indent level
            auto current_indent = base.token.size();
            auto c = state.new_indent_no_scope(s, current_indent);

            auto collect_comments = [&] {
                // get comments and add to scope
                auto comment = s.get_comments();
                if (comment) {
                    match->branch.push_back(std::move(comment));
                }
            };

            collect_comments();

            // Parse and add the first element
            match->branch.push_back(parse_match_branch());

            // Parse and add subsequent elements with the same indent level
            while (auto indent = s.peek_token(lexer::Tag::indent)) {
                if (indent->token.size() < current_indent) {
                    break;
                }
                s.must_consume_token(lexer::Tag::indent);
                match->branch.push_back(parse_match_branch());
            }

            push_union_to_current_struct();

            return match;
        }

        /*
            <if> ::= "if" <expr> <indent scope> ("elif" <expr> <block>)* ("else" <indent scope>)?
        */
        std::shared_ptr<If> parse_if(lexer::Token&& token) {
            s.skip_white();
            auto if_ = std::make_shared<If>(token.loc);

            auto cs = state.cond_scope(if_->cond_scope, if_);

            // 解析して if の条件式とブロックを設定
            if_->cond = parse_expr();
            std::shared_ptr<StructUnionType> union_ = std::make_shared<StructUnionType>(if_->loc);
            union_->base = if_;

            std::vector<std::shared_ptr<Expr>> cond;
            cond.push_back(if_->cond);

            auto push_union_to_current_struct = [&] {
                auto f = std::make_shared<Field>(if_->loc);
                f->field_type = union_;
                f->belong = state.current_member();
                state.add_to_struct(std::move(f));
                cs.execute();
                export_union_field(nullptr, cond, union_);
            };

            auto body_with_struct = [&](lexer::Loc loc, auto& block, const std::shared_ptr<If>& owner) {
                auto tmp = parse_indent_block(owner);
                union_->structs.push_back(tmp->struct_type);
                block = std::move(tmp);
            };

            body_with_struct(if_->loc, if_->then, if_);

            auto cur_indent = state.current_indent();

            auto detect_end = [&] {
                if (cur_indent != 0) {
                    auto indent_token = s.peek_token(lexer::Tag::indent);
                    if (!indent_token || indent_token->token.size() != cur_indent) {
                        return true;  // 次のインデントが現在のインデントと異なれば終了
                    }
                }
                return false;
            };

            auto consume_indent = [&] {
                if (cur_indent != 0) {
                    s.must_consume_token(lexer::Tag::indent);
                }
            };

            if (detect_end()) {
                push_union_to_current_struct();
                return if_;
            }

            consume_indent();  // elif or else のため次のインデントを消費

            // elif ブロックの解析
            std::shared_ptr<If> current_if = if_;
            while (auto tok = s.consume_token("elif")) {
                auto elif = std::make_shared<If>(tok->loc);
                s.skip_white();
                elif->cond = parse_expr();
                cond.push_back(elif->cond);
                body_with_struct(tok->loc, elif->then, elif);
                current_if->els = elif;
                current_if = std::move(elif);
                if (detect_end()) {
                    push_union_to_current_struct();
                    return if_;
                }
                consume_indent();  // else or elif のため次のインデントを消費
            }

            // else ブロックの解析
            if (s.consume_token("else")) {
                cond.push_back(nullptr);
                body_with_struct(if_->loc, current_if->els, current_if);
            }
            else {
                if (cur_indent != 0) {
                    // 同じインデントだが elifでもelseでもなかったので読んだインデントを戻しておく
                    s.backward();
                }
            }

            push_union_to_current_struct();

            return if_;
        }

        std::shared_ptr<Ident> parse_ident_no_scope() {
            auto token = s.must_consume_token(lexer::Tag::ident);
            return std::make_shared<Ident>(token.loc, std::move(token.token));
        }

        std::shared_ptr<Ident> parse_ident() {
            auto ident = parse_ident_no_scope();
            auto scope = state.current_scope();
            scope->push(ident);
            ident->scope = std::move(scope);
            return ident;
        }

        std::shared_ptr<Paren> parse_paren(lexer::Token&& token) {
            auto paren = std::make_shared<Paren>(token.loc);
            s.skip_white();
            paren->expr = parse_expr();
            s.skip_white();
            token = s.must_consume_token(")");
            paren->end_loc = token.loc;
            return paren;
        }

        std::shared_ptr<StrLiteral> parse_str_literal(lexer::Token&& lit) {
            auto literal = std::make_shared<StrLiteral>(lit.loc, std::move(lit.token));
            auto c = unescape_count(literal->value);
            if (!c) {
                s.report_error(lit.loc, "invalid string literal");
            }
            literal->length = *c;
            return literal;
        }

        std::shared_ptr<TypeLiteral> parse_type_literal(lexer::Token&& lit) {
            s.skip_line();
            auto typ = parse_type(false);
            s.skip_line();
            auto end_tok = s.must_consume_token(">");
            auto literal = std::make_shared<TypeLiteral>(lit.loc, std::move(typ), end_tok.loc);
            return literal;
        }

        /*
            <prim> ::= <int-literal> | <bool-literal> | <str-literal> | <ident> | "(" <expr> ")" | <if>
        */
        std::shared_ptr<Expr> parse_prim(bool* line_skipped) {
            if (auto token = s.consume_token(lexer::Tag::int_literal)) {
                return std::make_shared<IntLiteral>(token->loc, std::move(token->token));
            }
            if (auto b = s.consume_token(lexer::Tag::bool_literal)) {
                return std::make_shared<BoolLiteral>(b->loc, b->token == "true");
            }
            if (auto t = s.consume_token(lexer::Tag::str_literal)) {
                return parse_str_literal(std::move(*t));
            }
            if (auto i = s.consume_token("input")) {
                return std::make_shared<SpecialLiteral>(i->loc, state.input_type, SpecialLiteralKind::input_);
            }
            if (auto o = s.consume_token("output")) {
                return std::make_shared<SpecialLiteral>(o->loc, state.output_type, SpecialLiteralKind::output_);
            }
            if (auto c = s.consume_token("config")) {
                return std::make_shared<SpecialLiteral>(c->loc, state.config_type, SpecialLiteralKind::config_);
            }
            if (auto paren = s.consume_token("(")) {
                return parse_paren(std::move(*paren));
            }
            if (auto if_ = s.consume_token("if")) {
                if (line_skipped) {
                    *line_skipped = true;
                }
                return parse_if(std::move(*if_));
            }
            if (auto match = s.consume_token("match")) {
                if (line_skipped) {
                    *line_skipped = true;
                }
                return parse_match(std::move(*match));
            }
            if (auto typ = s.consume_token("<")) {
                return parse_type_literal(std::move(*typ));
            }
            if (auto i = s.peek_token(lexer::Tag::ident)) {
                auto i_desc = ast::is_int_type(i->token);
                auto f_desc = ast::is_float_type(i->token);
                if (i_desc || f_desc || i->token == "void" || i->token == "bool") {
                    std::shared_ptr<ast::Type> type;
                    if (i_desc) {
                        type = std::make_shared<ast::IntType>(i->loc, i_desc->bit_size, i_desc->endian, i_desc->is_signed);
                    }
                    else if (f_desc) {
                        type = std::make_shared<ast::FloatType>(i->loc, f_desc->bit_size, f_desc->endian);
                    }
                    else if (i->token == "void") {
                        type = std::make_shared<ast::VoidType>(i->loc);
                    }
                    else if (i->token == "bool") {
                        type = std::make_shared<ast::BoolType>(i->loc);
                    }
                    else {
                        assert(false);
                    }
                    auto type_literal = std::make_shared<TypeLiteral>(i->loc, std::move(type), i->loc);
                    s.must_consume_token(lexer::Tag::ident);
                    return type_literal;
                }
            }
            return parse_ident();
        }

        void collect_args(const std::shared_ptr<Expr>& args, auto& res) {
            if (auto a = ast::as<Binary>(args); a && a->op == ast::BinaryOp::comma) {
                collect_args(a->left, res);
                collect_args(a->right, res);
            }
            else {
                res.push_back(args);
            }
        }

        std::shared_ptr<Call> parse_call(lexer::Token&& token, std::shared_ptr<Expr>& p) {
            auto call = std::make_shared<Call>(token.loc, std::move(p));
            s.skip_white();
            if (!s.expect_token(")")) {
                call->raw_arguments = parse_expr();
                collect_args(call->raw_arguments, call->arguments);
            }
            token = s.must_consume_token(")");
            call->end_loc = token.loc;
            return call;
        }

        std::shared_ptr<Expr> parse_call_or_cast(lexer::Token&& token, std::shared_ptr<Expr>& p) {
            auto call = parse_call(std::move(token), p);
            if (auto typ = ast::as<ast::TypeLiteral>(call->callee)) {
                if (call->arguments.size() != 0 && call->arguments.size() != 1) {
                    s.report_error(call->loc, "expect 0 or 1 arguments for cast but got ", nums(call->arguments.size()));
                }
                std::shared_ptr<Expr> arg;
                if (call->arguments.size() == 1) {
                    arg = call->arguments[0];
                }
                auto copy = typ->type_literal;
                return std::make_shared<Cast>(std::move(call), std::move(copy), std::move(arg));
            }
            return call;
        }

        std::shared_ptr<Index> parse_index(lexer::Token&& token, std::shared_ptr<Expr>& p) {
            auto call = std::make_shared<Index>(token.loc, std::move(p));
            s.skip_white();
            call->index = parse_expr();
            token = s.must_consume_token("]");
            call->end_loc = token.loc;
            return call;
        }

        std::shared_ptr<MemberAccess> parse_access(lexer::Token&& token, std::shared_ptr<Expr>& p) {
            s.skip_white();
            auto ident = parse_ident_no_scope();
            ident->usage = IdentUsage::reference_member;
            auto member = std::make_shared<MemberAccess>(token.loc, std::move(p), std::move(ident));
            member->member->base = member;
            return member;
        }

        /*
            <post> ::= <prim>  ("(" <expr> ")" | <post> "." <ident> | <post> "[" <expr> "]")*
        */
        std::shared_ptr<Expr> parse_post(bool* line_skipped) {
            auto p = parse_prim(line_skipped);
            for (;;) {
                s.skip_space();
                if (auto c = s.consume_token("(")) {
                    p = parse_call_or_cast(std::move(*c), p);
                }
                else if (auto c = s.consume_token(".")) {
                    p = parse_access(std::move(*c), p);
                }
                else if (auto c = s.consume_token("[")) {
                    p = parse_index(std::move(*c), p);
                }
                else {
                    break;
                }
            }
            return p;
        }

        std::optional<lexer::Token> consume_op(size_t& i, auto& ops) {
            for (i = 0; i < ops.size(); i++) {
                if constexpr (futils::helper::is_template_instance_of<std::decay_t<decltype(ops[i])>, std::pair>) {
                    if (auto t = s.consume_token(ops[i].second)) {
                        return t;
                    }
                }
                else {
                    if (auto t = s.consume_token(ops[i])) {
                        return t;
                    }
                }
            }
            return std::nullopt;
        }

        /*
            <unary> ::= <post> | <unary-op> <unary>
            <unary-op> ::= <unary-op> | <unary-op> <unary-op> <unary>
        */
        std::shared_ptr<Expr> parse_unary(bool* line_skipped) {
            std::vector<std::shared_ptr<Unary>> stack;
            size_t i;
            s.skip_space();
            for (;;) {
                if (auto token = consume_op(i, ast::enum_array<UnaryOp>)) {
                    stack.push_back(std::make_shared<Unary>(token->loc, UnaryOp(i)));
                    s.skip_white();
                    continue;
                }
                break;
            }
            auto target = parse_post(line_skipped);  // return non-nullptr or throw error
            while (stack.size()) {
                auto ptr = std::move(stack.back());
                stack.pop_back();
                ptr->expr = std::move(target);
                target = std::move(ptr);
            }
            return target;
        }

        bool is_finally_ident(ast::Expr* expr) {
            if (expr->node_type == ast::NodeType::ident) {
                return true;
            }
            if (expr->node_type == ast::NodeType::member_access) {
                return is_finally_ident(static_cast<ast::MemberAccess*>(expr)->target.get());
            }
            if (expr->node_type == ast::NodeType::special_literal) {
                return true;
            }
            return false;
        }

        void check_duplicated_def(ast::Ident* ident) {
            auto found = ident->scope->lookup_current(
                [&](std::shared_ptr<ast::Ident>& i) {
                    if (i->usage != ast::IdentUsage::unknown && i->ident == ident->ident) {
                        return true;
                    }
                    return false;
                },
                ident);
            if (found) {
                error(ident->loc, "duplicate definition of ", ident->ident)
                    .error((*found)->loc, "previous definition is here")
                    .report();
            }
        }

        void check_assignment(const std::shared_ptr<Binary>& assign) {
            if (assign->op == ast::BinaryOp::define_assign ||
                assign->op == ast::BinaryOp::const_assign) {
                auto ident = ast::as<ast::Ident>(assign->left);
                if (!ident) {
                    s.report_error(assign->left->loc, "left of := or ::= must be ident");
                }
                ident->usage = assign->op == ast::BinaryOp::define_assign ? ast::IdentUsage::define_variable : ast::IdentUsage::define_const;
                ident->base = assign;
                check_duplicated_def(ident);
            }
            else if (assign->op == ast::BinaryOp::assign) {
                if (!is_finally_ident(assign->left.get())) {
                    s.report_error(assign->left->loc, "left of = must be ident, member access or input/output/config");
                }
            }
        }

        struct BinOpStack {
            size_t depth = 0;
            std::shared_ptr<Expr> expr;
        };

        bool appear_valid_range_end() {
            for (auto u : enum_array<ast::UnaryOp>) {
                if (s.expect_token(u.second)) {
                    return true;
                }
            }
            if (s.expect_token(lexer::Tag::ident) ||
                s.expect_token(lexer::Tag::bool_literal) ||
                s.expect_token(lexer::Tag::int_literal) ||
                s.expect_token(lexer::Tag::str_literal) ||
                s.expect_token("input") || s.expect_token("output") ||
                s.expect_token("if") || s.expect_token("match")) {
                return true;
            }
            return false;
        }

        /*
            <expr> ::= <unary> | <unary> <bin-op> <expr>
            <bin-op> ::= <bin-op> | <bin-op> <bin-op> <expr>
        */
        std::shared_ptr<Expr> parse_expr(bool* line_skipped = nullptr) {
            std::shared_ptr<Expr> expr;
            size_t depth;
            std::vector<BinOpStack> stack;
            size_t i;
            auto parse_low = [&] {
                expr = parse_unary(line_skipped);  // return non-nullptr or throw error
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
                    if (op.expr->node_type == NodeType::binary) {
                        if (depth == ast::bin_assign_layer) {
                            stack.push_back(std::move(op));
                        }
                        else {
                            auto b = static_cast<Binary*>(op.expr.get());
                            b->right = std::move(expr);
                            expr = std::move(op.expr);
                        }
                    }
                    else if (op.expr->node_type == NodeType::range) {
                        auto b = static_cast<Range*>(op.expr.get());
                        b->end = std::move(expr);
                        expr = std::move(op.expr);
                    }
                    else if (op.expr->node_type == NodeType::cond) {
                        auto cop = static_cast<Cond*>(op.expr.get());
                        if (!cop->then) {
                            cop->then = std::move(expr);
                            s.skip_white();
                            auto token = s.must_consume_token(":");
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
                            s.report_error("expect cond with `cond` and `then` but not; parser bug");
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
                        check_assignment(ast::cast_to<Binary>(op.expr));
                        expr = std::move(op.expr);
                    }
                }
            };

            // treat range expression specially
            if (!s.expect_token("..") && !s.expect_token("..=")) {
                parse_low();  // first time
            }
            else {
                depth = 0;
            }

            while (depth < ast::bin_layer_len) {
                if (update_stack()) {
                    continue;
                }
                s.skip_space();
                if (depth == ast::bin_cond_layer) {
                    if (auto token = s.consume_token("?")) {
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
                        if (depth == ast::bin_range_layer) {
                            if (auto bin = as<Range>(expr); bin && is_range_op(bin->op)) {
                                // this is like `a .. b .. c` but this language not support it
                                s.report_error(token->loc, "unexpected `", token->token, "`");
                            }
                            s.skip_space();  // for safety, skip only space, not line
                            auto r = std::make_shared<Range>(token->loc, std::move(expr), *ast::from_string<ast::BinaryOp>(ast::bin_layers[depth][i]));
                            if (appear_valid_range_end()) {
                                s.skip_white();
                                stack.push_back(BinOpStack{.depth = depth, .expr = std::move(r)});
                                parse_low();
                                continue;
                            }
                            expr = std::move(r);
                            continue;
                        }
                        s.skip_white();
                        auto b = std::make_shared<Binary>(token->loc, std::move(expr), *ast::from_string<ast::BinaryOp>(ast::bin_layers[depth][i]));
                        if (depth == 0) {                          // special case, needless to use stack
                            b->right = parse_unary(line_skipped);  // return non-nullptr or throw error
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

        /*
            <loop> ::= "loop" <expr>? (";" <expr>?)? (";" <expr>?)? <indent block>
        */
        std::shared_ptr<Loop> parse_for(lexer::Token&& token) {
            auto for_ = std::make_shared<Loop>(token.loc);
            auto cs = state.cond_scope(for_->cond_scope, for_);
            s.skip_white();
            if (s.expect_token(":")) {
                for_->body = parse_indent_block(for_);
                return for_;
            }
            if (!s.expect_token(";")) {
                for_->init = parse_expr();
                s.skip_white();
            }
            if (s.expect_token(":")) {
                for_->cond = std::move(for_->init);
                for_->body = parse_indent_block(for_);
                return for_;
            }
            s.must_consume_token(";");
            s.skip_white();
            if (!s.expect_token(";")) {
                for_->cond = parse_expr();
                s.skip_white();
            }
            if (s.expect_token(":")) {
                for_->body = parse_indent_block(for_);
                return for_;
            }
            s.must_consume_token(";");
            s.skip_white();
            if (!s.expect_token(":")) {
                for_->step = parse_expr();
                s.skip_white();
            }
            for_->body = parse_indent_block(for_);
            return for_;
        }

        /*
            <func type> ::= "fn" "(" (<type> ("," <type>)*)? ")" ("->" <type>)?
        */
        // fn (a :int,b :int) -> int
        std::shared_ptr<FunctionType> parse_func_type(lexer::Token&& tok) {
            auto func_type = std::make_shared<FunctionType>(tok.loc, true);
            s.skip_white();
            s.must_consume_token("(");
            s.skip_white();
            bool second = false;
            while (!s.expect_token(")")) {
                if (second) {
                    s.must_consume_token(",");
                }
                if (s.consume_token(lexer::Tag::ident)) {
                    s.skip_white();
                }
                s.must_consume_token(":");
                s.skip_white();
                func_type->parameters.push_back(parse_type(true));
                s.skip_white();
                second = true;
            }
            s.must_consume_token(")");
            s.skip_space();  // for safety, skip only space, not line
            if (s.consume_token("->")) {
                s.skip_white();
                func_type->return_type = parse_type(true);
            }
            return func_type;
        }

        /*
            <type> ::= <array type> | <int type> | <str type> | <func type> | <ident type>
            <array type> ::= "[" <expr>? "]" <type>
            <int literal type> ::= <int literal>
            <str literal type> ::= <str literal>
            <ident type> ::= <ident>
        */
        std::shared_ptr<Type> parse_type(bool as_argument) {
            if (auto arr_begin = s.consume_token("[")) {
                s.skip_white();
                std::shared_ptr<Expr> expr;
                if (!s.expect_token("]")) {
                    expr = parse_expr();
                    s.skip_white();
                }
                auto end_tok = s.must_consume_token("]");
                s.skip_space();
                auto base_type = parse_type(as_argument);
                return std::make_shared<ArrayType>(arr_begin->loc, std::move(expr), end_tok.loc, std::move(base_type), true);
            }

            if (auto lit = s.consume_token(lexer::Tag::str_literal)) {
                return std::make_shared<StrLiteralType>(std::move(parse_str_literal(std::move(*lit))), true);
            }

            if (auto fn = s.consume_token("fn")) {
                return parse_func_type(std::move(*fn));
            }

            if (auto void_ = s.consume_token("void")) {
                return std::make_shared<VoidType>(void_->loc, true);
            }

            if (auto bool_ = s.consume_token("bool")) {
                return std::make_shared<BoolType>(bool_->loc, true);
            }

            auto ident = s.must_consume_token(lexer::Tag::ident);

            if (auto desc = is_int_type(ident.token)) {
                return std::make_shared<IntType>(ident.loc, desc->bit_size, desc->endian, desc->is_signed, true);
            }
            if (auto desc = is_float_type(ident.token)) {
                return std::make_shared<FloatType>(ident.loc, desc->bit_size, desc->endian, true);
            }

            auto base = std::make_shared<Ident>(ident.loc, std::move(ident.token));
            base->usage = IdentUsage::maybe_type;
            base->scope = state.current_scope();

            auto id = std::make_shared<IdentType>(ident.loc, std::move(base));
            if (!as_argument) {
                if (auto fmt = ast::as<ast::Format>(state.current_member())) {
                    fmt->depends.push_back(id);
                }
            }
            return id;
        }

        /*
            <field type> ::= ":" <type> ("(" <expr> ")")?
        */
        // may returns expr if not field
        std::shared_ptr<Node> parse_field(const std::shared_ptr<Expr>& expr, bool as_argument) {
            lexer::Token token;
            std::shared_ptr<Ident> ident;
            if (expr) {
                if (expr->node_type != NodeType::ident) {
                    return expr;
                }
                s.skip_space();
                auto tmp = s.consume_token(":");
                if (!tmp) {
                    return expr;
                }
                ident = cast_to<Ident>(expr);
                token = std::move(*tmp);
            }
            else {
                token = s.must_consume_token(":");
            }

            auto field = std::make_shared<Field>(ident ? ident->loc : token.loc);
            field->colon_loc = token.loc;

            field->ident = std::move(ident);
            s.skip_space();

            field->field_type = parse_type(as_argument);

            if (field->ident) {
                field->ident->expr_type = field->field_type;
                field->ident->base = field;
                field->ident->usage = IdentUsage::define_field;
                field->ident->constant_level = ConstantLevel::variable;
                check_duplicated_def(field->ident.get());
            }

            field->belong = state.current_member();

            if (auto b = s.consume_token("(")) {
                s.skip_white();

                auto field_argument = std::make_shared<FieldArgument>(b->loc);

                if (!s.expect_token(")")) {
                    field_argument->raw_arguments = parse_expr();
                    collect_args(field_argument->raw_arguments, field_argument->collected_arguments);
                    s.skip_white();
                }

                auto e = s.must_consume_token(")");
                field_argument->end_loc = e.loc;

                field->arguments = std::move(field_argument);
            }

            state.add_to_struct(field);

            return field;
        }

        /*
            <enum> ::= "enum" <ident> ":\r\n" (":"<type>)? <enum member>+
            <enum member> ::= <indent> <ident> ("=" <expr>)?
         */
        std::shared_ptr<Enum> parse_enum(lexer::Token&& token) {
            auto enum_ = std::make_shared<Enum>(token.loc);
            s.skip_white();
            enum_->ident = parse_ident();
            enum_->ident->usage = IdentUsage::define_enum;
            enum_->ident->base = enum_;
            check_duplicated_def(enum_->ident.get());
            enum_->enum_type = std::make_shared<EnumType>(enum_->loc);
            enum_->enum_type->base = enum_;
            must_consume_indent_sign();
            auto base = s.must_consume_token(lexer::Tag::indent);
            auto m_scope = state.enter_member(enum_);
            auto s_scope = state.new_indent(s, base.token.size(), enum_->scope, enum_);
            if (auto tok = s.consume_token(":")) {
                s.skip_white();
                enum_->base_type = parse_type(false);
                s.skip_space();
                s.must_consume_token(lexer::Tag::line);
                s.skip_line();
                auto indent = s.must_consume_token(lexer::Tag::indent);
                if (indent.token.size() != base.token.size()) {
                    s.report_error(indent.loc, "indent size must be same as enum base");
                }
            }
            auto parse_enum_member = [&] {
                auto ident = parse_ident();
                ident->usage = IdentUsage::define_enum_member;
                ident->expr_type = enum_->enum_type;
                ident->constant_level = ConstantLevel::constant;
                check_duplicated_def(ident.get());
                auto member = std::make_shared<EnumMember>(ident->loc);
                member->ident = ident;
                member->belong = enum_;
                ident->base = member;
                s.skip_space();
                if (s.consume_token("=")) {
                    s.skip_white();
                    member->expr = parse_expr();
                }
                member->comment = s.get_comments();
                enum_->members.push_back(member);
                s.skip_line();
            };
            parse_enum_member();
            while (auto indent = s.peek_token(lexer::Tag::indent)) {
                if (indent->token.size() != base.token.size()) {
                    break;
                }
                s.must_consume_token(lexer::Tag::indent);
                parse_enum_member();
            }
            state.add_to_struct(enum_);
            return enum_;
        }

        /*
            <format> ::= "format" <ident> <indent block>
        */
        std::shared_ptr<Format> parse_format(lexer::Token&& token) {
            auto fmt = std::make_shared<Format>(token.loc);
            s.skip_white();

            fmt->ident = parse_ident();
            fmt->ident->usage = IdentUsage::define_format;
            fmt->ident->base = fmt;
            check_duplicated_def(fmt->ident.get());
            {
                auto m_scope = state.enter_member(fmt);
                fmt->body = parse_indent_block(fmt);
            }
            // because fmt->ident->expr_type = fmt->body->struct_type
            // makes circular reference, so not use it
            state.add_to_struct(fmt);

            // fetch encode_fn and decode_fn
            auto enc = fmt->body->struct_type->lookup("encode");
            if (auto fn = as<Function>(enc)) {
                fmt->encode_fn = cast_to<Function>(enc);
            }
            auto dec = fmt->body->struct_type->lookup("decode");
            if (auto fn = as<Function>(dec)) {
                fmt->decode_fn = cast_to<Function>(dec);
            }
            // lookup cast fn
            fmt->body->struct_type->lookup([&](std::shared_ptr<Member>& m) {
                if (auto fn = ast::as<ast::Function>(m); fn && fn->is_cast) {
                    fmt->cast_fns.push_back(ast::cast_to<ast::Function>(m));
                }
                return false;
            });

            return fmt;
        }

        /*
            <state> ::= "state" <ident> <indent block>
        */
        std::shared_ptr<State> parse_state(lexer::Token&& token) {
            auto state_ = std::make_shared<State>(token.loc);
            s.skip_white();

            state_->ident = parse_ident();
            state_->ident->usage = IdentUsage::define_state;
            state_->ident->base = state_;
            check_duplicated_def(state_->ident.get());
            {
                auto m_scope = state.enter_member(state_);
                state_->body = parse_indent_block(state_);
            }
            // state_->ident->expr_type = state_->body->struct_type
            // makes circular reference, so not use it
            state.add_to_struct(state_);

            return state_;
        }

        /*
            <fn> ::= fn "cast"? <ident> "(" (<ident> : <type> ("," <ident > <type>)*)? ")" ("->" <type>)? <indent block>
        */
        std::shared_ptr<Function> parse_fn(lexer::Token&& token) {
            auto fn = std::make_shared<Function>(token.loc);
            s.skip_white();
            if (auto cast = s.consume_token("cast")) {
                fn->is_cast = true;
                fn->cast_loc = cast->loc;
                s.skip_white();
            }
            fn->ident = parse_ident();
            fn->ident->usage = fn->is_cast ? IdentUsage::define_cast_fn : IdentUsage::define_fn;
            fn->ident->base = fn;
            check_duplicated_def(fn->ident.get());
            fn->ident->constant_level = ConstantLevel::constant;
            fn->belong = state.current_member();
            fn->func_type = std::make_shared<FunctionType>(fn->loc);
            fn->ident->expr_type = fn->func_type;
            s.skip_white();
            lexer::Loc end_loc;
            std::vector<std::shared_ptr<Ident>> ident_param;
            auto b = s.must_consume_token("(");
            for (;;) {
                s.skip_white();
                if (auto t = s.consume_token(")")) {
                    end_loc = t->loc;
                    break;
                }
                std::shared_ptr<Ident> ident;
                if (!s.expect_token(":")) {
                    ident = parse_ident_no_scope();
                    ident->usage = IdentUsage::define_arg;
                    ident->scope = state.current_scope();
                    ident_param.push_back(ident);
                    s.skip_white();
                }
                if (!s.expect_token(":")) {
                    s.must_consume_token(":");
                }

                auto f = parse_field(std::move(ident), true);
                auto field = cast_to<Field>(f);
                fn->parameters.push_back(field);
                fn->func_type->parameters.push_back(field->field_type);
                if (s.expect_token(")") || s.consume_token(",")) {
                    continue;
                }
                s.must_consume_token(",");  // this must be error
            }
            s.skip_white();
            if (auto r = s.consume_token("->")) {
                s.skip_white();
                fn->return_type = parse_type(false);
            }
            else {
                fn->return_type = std::make_shared<VoidType>(end_loc);
            }

            fn->func_type->return_type = fn->return_type;

            state.add_to_struct(fn);

            {
                auto m_scope = state.enter_member(fn);
                // auto typ = state.enter_struct(fn->struct_type);
                fn->body = parse_indent_block(fn, &ident_param);
            }

            return fn;
        }

        /*
            <statement> ::= <for> | <if> | <format> | <fn> | <return> | <break> | <continue> | <expr or field>
            <expr or field> ::= <expr>? <field type>
        */
        std::shared_ptr<Node> parse_statement(bool* prev_skip_line = nullptr) {
            auto set_skip = [&] {
                if (prev_skip_line) {
                    *prev_skip_line = true;
                }
            };
            auto skip_last = [&] {
                s.skip_space();

                if (!s.eos() && s.expect_token(lexer::Tag::line)) {
                    s.must_consume_token(lexer::Tag::line);
                    s.skip_line();
                    set_skip();
                }
            };

            if (auto loop = s.consume_token("loop")) {
                set_skip();
                return parse_for(std::move(*loop));
            }

            if (auto format = s.consume_token("format")) {
                set_skip();
                return parse_format(std::move(*format));
            }

            if (auto state_ = s.consume_token("state")) {
                set_skip();
                return parse_state(std::move(*state_));
            }

            if (auto enum_ = s.consume_token("enum")) {
                set_skip();
                return parse_enum(std::move(*enum_));
            }

            if (auto fn = s.consume_token("fn")) {
                set_skip();
                return parse_fn(std::move(*fn));
            }

            if (auto ret = s.consume_token("return")) {
                auto ret_ = std::make_shared<Return>(ret->loc);
                s.skip_space();
                if (!s.eos() && !s.consume_token(lexer::Tag::line)) {
                    ret_->expr = parse_expr(prev_skip_line);
                }
                skip_last();
                return ret_;
            }

            if (auto br = s.consume_token("break")) {
                skip_last();
                return std::make_shared<Break>(br->loc);
            }

            if (auto cont = s.consume_token("continue")) {
                skip_last();
                return std::make_shared<Continue>(cont->loc);
            }

            std::shared_ptr<Node> node;
            if (s.expect_token(":")) {
                node = parse_field(nullptr, false);
            }
            else {
                auto expr = parse_expr(prev_skip_line);
                node = parse_field(expr, false);
            }

            skip_last();

            return node;
        }
    };
}  // namespace brgen::ast
