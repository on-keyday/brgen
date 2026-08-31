/*license*/
#include "core/lexer/token.h"
#include "../node/nodes.h"
#include "../node/access.h"
#include "../node/util.h"
#include "stream.h"
// #include "strutil/append.h"
#include "parse.h"
#include <fnet/util/base64.h>
#include "../node/builtin.h"

namespace brgen::nast {

    constexpr auto must_success = "THIS MUST SUCCESS; if shown, parser bug!!";

    /*
    struct ParserState {
        bool collect_comment = false;
        bool error_tolerant = false;
        LocationError& errors;  // for error tolerant mode

        ParserState(LocationError& errors)
            : errors(errors) {
        }

       private:
        size_t indent = 0;
        ScopeStack stack;
        Node<NamedStatement> current_fmt_;
        Node<StructType> current_struct_;

       public:
        auto cond_scope(scope_ptr& frame, const Node<Node>& scope_owner) {
            auto br = stack.enter_branch();
            frame = stack.current_scope();
            frame->owner = scope_owner;
            // initialize extent to owner's head loc; callers may widen begin/end later
            // (parse_indent_block sets begin to the first body indent and end to the
            // last body element).
            if (scope_owner) {
                frame->loc = scope_owner->loc;
            }
            return br;
        }

        auto new_indent(Stream& s, size_t new_, scope_ptr& frame, const Node<Node>& scope_owner) {
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

        auto enter_member(const Node<NamedStatement>& f) {
            f->belong = current_fmt_;
            current_fmt_ = f;
            return futils::helper::defer([this] {
                current_fmt_ = current_fmt_->belong.lock();
            });
        }

        auto enter_struct(const Node<StructType>& type) {
            auto tmp = current_struct_;
            current_struct_ = type;
            return futils::helper::defer([this, tmp] {
                current_struct_ = std::move(tmp);
            });
        }

        Node<NamedStatement> current_member() {
            return current_fmt_;
        }

        void add_to_struct(const Node<NamedStatement>& f) {
            f->belong_struct = current_struct_;
            if (auto field = f.as<Field>()) {
                for (auto it = current_struct_->fields.rbegin(); it != current_struct_->fields.rend(); it++) {
                    if (auto p = as<Field>(*it)) {
                        p->next = (f).as<Field>();
                        break;
                    }
                }
            }
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
    */

    struct ParserState {
        bool collect_comment = false;
        bool error_tolerant = false;
        LocationError& errors;  // for error tolerant mode

        ParserState(LocationError& errors)
            : errors(errors) {
        }

       private:
        size_t indent = 0;
        // ScopeStack stack;
        // Node<NamedStatement> current_fmt_;
        // Node<StructType> current_struct_;

       public:
        /*
        auto cond_scope(scope_ptr& frame, const Node<Node>& scope_owner) {
            auto br = stack.enter_branch();
            frame = stack.current_scope();
            frame->owner = scope_owner;
            // initialize extent to owner's head loc; callers may widen begin/end later
            // (parse_indent_block sets begin to the first body indent and end to the
            // last body element).
            if (scope_owner) {
                frame->loc = scope_owner->loc;
            }
            return br;
        }
        */

        auto new_indent_no_scope(Stream& s, size_t new_) {
            if (indent >= new_) {
                s.report_error("expect larger indent but not");
            }
            auto old = std::exchange(indent, std::move(new_));
            return futils::helper::defer([=, this] {
                indent = std::move(old);
            });
        }

        /*
        Node<NamedStatement> current_member() {
            return current_fmt_;
        }
        */

        size_t current_indent() {
            return indent;
        }
    };

    struct Parser {
        Stream& s;
        // ParserState state;
        ParserState state;
        Arena& a;

        Parser(Stream& s, LocationError& errors, Arena& arena)
            : s(s), state(errors), a(arena) {
        }

        Node<Module> parse() {
            s.set_regex_mode(true);  // default regex mode is on
            auto prog = a.make<Module>();
            prog.set_loc(s.loc());
            // prog->global_scope = state.reset_stack();
            // prog->global_scope->owner = prog;
            //  global_scope spans the whole file; begin = 0, end is updated after
            //  all top-level statements are parsed.
            // prog->global_scope->loc = prog.loc();
            // prog->global_scope->loc.pos.begin = 0;
            // prog->struct_type = a.make<StructType>(prog.loc());
            // prog->struct_type->base = prog;
            // auto st = state.enter_struct(prog->struct_type);
            s.skip_line();
            /*
            Node<Statement> comment;
            auto collect_comments = [&] {
                // get comments and add to scope
                comment = s.get_comments();
            };
            */
            while (!s.eos()) {
                //  collect_comments();
                auto stmt = parse_statement(nullptr, true);
                /*
                if (auto member = expr.as<NamedStatement>()) {
                    member->comment = std::move(comment);
                }
                else {
                    // if top level statement is not member, add comment to program
                    if (comment) {
                        prog->elements.push_back(std::move(comment));
                    }
                }
                */
                prog->statements.push_back(stmt);
                s.shrink();
                s.skip_white();
            }
            /*
            if (!prog->elements.empty()) {
                prog->global_scope->loc.pos.end = prog->elements.back()->loc.pos.end;
            }
            */
            return prog;
        }

       private:
        friend struct ParserTest;

        // :\\r\\n
        void must_consume_indent_sign(std::string_view hint) {
            s.skip_white();
            auto msg = futils::strutil::concat<std::string>(hint, ", only `:` is needed");
            if (state.error_tolerant) {
                auto indent = s.consume_token(":");
                if (!indent) {
                    auto token = s.token_error(":", msg);
                    state.errors.locations.insert(state.errors.locations.end(), token.locations.begin(), token.locations.end());
                    s.recover_to_prev_skip();
                }
            }
            else {
                s.must_consume_token(":", msg);
            }
            s.skip_space();
            s.consume_token(lexer::Tag::comment);  // optional comment after ':'
            // auto follow_comment = s.get_comments();
            s.must_consume_token(lexer::Tag::line, "line expected after ':'");
            s.skip_line();
            // return follow_comment;
        }

        /*
            <indent scope> ::= ":" <line> (<indent> <statement>)+
        */
        Node<Body> parse_indent_block(Node<Statement> scope_owner, std::string_view hint, std::vector<Node<Ident>>* ident = nullptr) {
            // Consume the initial indent sign
            must_consume_indent_sign(hint);

            // Get the base indent token
            auto base = s.must_consume_token(lexer::Tag::indent, "indent expected after ':'");

            // Create a shared pointer for the Body
            auto block = a.make<Body>(base.loc);

            // block->follow_comment = std::move(follow_comment);
            // block->struct_type = a.make<StructType>(base.loc);

            assert(scope_owner != nullref);
            // block->struct_type.ref(a)->base = scope_owner;

            // Create a new context for the current indent level
            auto current_indent = base.loc.pos.len();
            auto c = state.new_indent_no_scope(s, current_indent);
            // auto ss = state.enter_struct(block->struct_type);

            /*
            if (ident) {
                for (auto& i : *ident) {
                    // i->scope = block->scope;
                    // block->scope->push(i);
                    // check_duplicated_def(i.ref(a).get());
                }
            }

            Node<Node> comment;
            auto collect_comments = [&] {
                // get comments and add to scope
                comment = s.get_comments();
            };
            */

            auto parse_a_line = [&] {
                // collect_comments();
                bool line_skipped = false;
                while (!line_skipped) {
                    auto expr = parse_statement(&line_skipped);
                    /*
                    if (auto member = expr.as<NamedStatement>()) {
                        member->comment = std::move(comment);
                    }
                    else {
                        if (comment) {
                            block->elements.push_back(std::move(comment));
                        }
                    }
                    */
                    block->statements.push_back(std::move(expr));
                    if (s.peek_token(lexer::Tag::indent) || s.eos()) {
                        break;
                    }
                }
            };

            // Parse and add the first element
            parse_a_line();

            // Parse and add subsequent elements with the same indent level
            while (auto indent = s.peek_token(lexer::Tag::indent)) {
                if (indent->loc.pos.len() != current_indent) {
                    break;
                }
                s.must_consume_token(lexer::Tag::indent, "to start a new line in indent block");
                parse_a_line();
            }

            // ブロックの範囲を残す。始まりは Body 自身の loc (最初の indent)、
            // 終わりがここ。language server が位置からスコープを引くのに要る
            // (元は Scope::loc が持っていた。持ち主の loc は先頭トークンしか
            //  指さないので、それだけでは範囲にならない)。
            block->end_loc = base.loc;
            if (!block->statements.empty()) {
                block->end_loc = block->statements.back().ref(a).loc();
            }

            return block;
        }

        /*
        void export_union_field(Node<Identity>& cond0, std::vector<Node<Identity>>& cond, const Node<StructUnionType>& type) {
            assert(cond.size() == type->structs.size());
            type->cond = cond0;
            for (auto& c : cond) {
                type->conds.push_back(c);
            }
            std::map<std::string, std::vector<Node<UnionCandidate>>> m;
            for (size_t i = 0; i < type->conds.size(); i++) {
                auto& c = type->conds[i];
                auto& f = type->structs[i];
                for (auto& d : f->fields) {
                    if (!d->name) {
                        continue;
                    }
                    if (!d.as<Field>()) {
                        continue;
                    }
                    if (auto found = m.find(d->name->name); found != m.end()) {
                        for (auto& c1 : found->second) {
                            if (c1->cond.lock() == c) {
                                error(d->loc, "duplicate field name: ", d->name->name).error(c->loc, "previous definition is here").report();
                            }
                        }
                    }
                    auto cand = a.make<UnionCandidate>(d->loc);
                    cand->cond = c;
                    cand->field = (d).as<Field>();
                    m[d->name->name].push_back(std::move(cand));
                }
            }
            std::vector<Node<UnionCandidate>> null_cache;
            auto get_null_cache = [&](size_t i) {
                assert(i < type->conds.size());
                if (null_cache.size() <= i) {
                    null_cache.resize(i + 1);
                }
                if (!null_cache[i]) {
                    null_cache[i] = a.make<UnionCandidate>(type->loc);
                    null_cache[i].ref(a)->cond = type->conds[i];
                }
                return null_cache[i];
            };
            for (auto& [k, v] : m) {
                auto union_type = a.make<UnionType>();
                union_type->cond = cond0;
                union_type.set_loc(v[0].ref(a).loc());
                auto ident = a.make<Ident>(union_type.loc(), k);
                ident->usage = IdentUsage::define_field;
                ident->scope = state.current_scope();
                ident->scope->push(ident);
                ident->expr_type = union_type;
                auto field = a.make<Field>(union_type.loc());
                field->name = ident;
                ident->base = field;
                field->field_type = union_type;
                field->belong = state.current_member();
                union_type->base_type = type;
                size_t cand_i = 0;
                for (auto& c : v) {
                    while (c->cond.lock() != type->conds[cand_i]) {
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
        */

        /*
            <match> ::= "match" <expr>? <match branch>*
            <match branch> ::= <expr> [":" <indent block> | "=>" <statement>]
        */
        Node<Match> parse_match(lexer::LiteToken&& token) {
            // Create a shared pointer for the Match
            auto match = a.make<Match>(token.loc);

            // Node<StructUnionType> union_ = a.make<StructUnionType>(match.loc());
            // match->struct_union_type = union_;
            // union_.ref(a)->base = match;

            // std::vector<Node<Identity>> cond;

            // auto cs = state.cond_scope(match->cond_scope, match);

            /*
            auto push_union_to_current_struct = [&] {
                auto f = a.make<Field>(match.loc());
                f->field_type = union_;
                f->belong = state.current_member();
                state.add_to_struct(std::move(f));
                cs.execute();
                export_union_field(match->cond, cond, union_);
            };
            */

            s.skip_white();

            if (!s.expect_token(":")) {
                match->condition = parse_expr_identity();
            }

            // Consume the initial indent sign
            must_consume_indent_sign("to start match branch");

            auto stmt_with_struct = [&](lexer::Loc loc, Ref<ConditionalStatement>& br) {
                // auto scoped = a.make<ScopedStatement>(loc);
                // scoped->struct_type = a.make<StructType>(loc);
                // scoped->struct_type.ref(a)->base = br;
                // auto s_scope = state.enter_struct(scoped->struct_type);
                // auto c_scope = state.cond_scope(scoped->scope, br);
                auto bdy = a.make<Body>(loc);
                auto tmp_push_0_ = parse_statement();
                bdy->statements.push_back(tmp_push_0_);
                br->body = bdy;
                // union_.ref(a)->structs.push_back(scoped->struct_type);
                // br.ref(a)->then = std::move(scoped);
            };

            /*
            auto collect_comments = [&](Node<MatchBranch>& b) {
                b.ref(a)->comment = s.get_comments();
            };
            */

            auto parse_match_branch = [&]() -> Node<BodyStatement> {
                auto br = a.make<ConditionalStatement>();
                br->belong = match;
                br->condition = parse_expr_identity();
                br.set_loc(br->condition.ref(a).loc());
                // collect_comments(br);
                if (auto b = br->condition.as<Binary>().ref(a); b && b->op == BinaryOp::comma) {
                    auto c = a.make<OrCond>(b.loc());
                    c->base = br->condition.as<Binary>();
                    collect_comma(c->base, c->conds);
                    br->condition = c;
                }
                // cond.push_back(br->cond);
                // br.set_loc(br->cond.ref(a).loc());
                s.skip_white();
                auto sym = s.consume_token("=>");
                if (!sym) {
                    auto tok = s.peek_token(":");
                    auto block = parse_indent_block(br, "to start block style match branch");
                    // union_.ref(a)->structs.push_back(block.ref(a)->struct_type);
                    br->body = block;
                    // br->sym_loc = tok->loc;
                    return br;
                }
                // br->sym_loc = sym->loc;
                s.skip_white();
                stmt_with_struct(sym->loc, br);
                return br;
            };

            // Get the base indent token
            auto base = s.must_consume_token(lexer::Tag::indent, "indent expected after `match`");

            // Create a new context for the current indent level
            auto current_indent = base.loc.pos.len();
            // auto c = state.new_indent_no_scope(s, current_indent);

            // Parse and add the first element
            auto tmp_push_1_ = parse_match_branch();
            match->blocks.push_back(tmp_push_1_);

            // Parse and add subsequent elements with the same indent level
            while (auto indent = s.peek_token(lexer::Tag::indent)) {
                if (indent->loc.pos.len() < current_indent) {
                    break;
                }
                s.must_consume_token(lexer::Tag::indent, "to start a new line in match branch");
                auto tmp_push_2_ = parse_match_branch();
                match->blocks.push_back(tmp_push_2_);
            }

            // push_union_to_current_struct();

            return match;
        }

        /*
            <if> ::= "if" <expr> <indent scope> ("elif" <expr> <block>)* ("else" <indent scope>)?
        */
        Node<If> parse_if(lexer::LiteToken&& token) {
            s.skip_white();
            auto if_ = a.make<If>(token.loc);

            // auto cs = state.cond_scope(if_->cond_scope, if_);

            auto then = a.make<ConditionalStatement>(if_.loc());
            then->belong = if_;
            if_->blocks.push_back(then);

            // 解析して if の条件式とブロックを設定
            then->condition = parse_expr_identity();
            // Node<StructUnionType> union_ = a.make<StructUnionType>(if_.loc());
            // if_->struct_union_type = union_;
            // union_.ref(a)->base = if_;

            // std::vector<Node<Identity>> cond;
            // cond.push_back(if_->cond);

            /*
            auto push_union_to_current_struct = [&] {
                auto f = a.make<Field>(if_.loc());
                f->field_type = union_;
                f->belong = state.current_member();
                state.add_to_struct(std::move(f));
                cs.execute();
                export_union_field(nullptr, cond, union_);
            };
            */

            auto body_with_struct = [&](Ref<BodyStatement> owner, std::string_view hint) {
                owner->body = parse_indent_block(owner, hint);
                // union_.ref(a)->structs.push_back(tmp.ref(a)->struct_type);
            };

            body_with_struct(then, "to start `if` body");

            auto cur_indent = state.current_indent();

            auto detect_end = [&] {
                if (cur_indent != 0) {
                    auto indent_token = s.peek_token(lexer::Tag::indent);
                    if (!indent_token || indent_token->loc.pos.len() != cur_indent) {
                        return true;  // 次のインデントが現在のインデントと異なれば終了
                    }
                }
                return false;
            };

            auto consume_indent = [&] {
                if (cur_indent != 0) {
                    s.must_consume_token(lexer::Tag::indent, "to start a new line in if block");
                }
            };

            if (detect_end()) {
                // push_union_to_current_struct();
                return if_;
            }

            consume_indent();  // elif or else のため次のインデントを消費

            // elif ブロックの解析
            while (auto tok = s.consume_token("elif")) {
                auto elif = a.make<ConditionalStatement>(tok->loc);
                elif->belong = if_;
                if_->blocks.push_back(elif);
                s.skip_white();
                elif->condition = parse_expr_identity();
                // cond.push_back(elif->cond);
                body_with_struct(elif, "to start `elif` body");
                if (detect_end()) {
                    // push_union_to_current_struct();
                    return if_;
                }
                consume_indent();  // else or elif のため次のインデントを消費
            }

            // else ブロックの解析
            if (auto l = s.consume_token("else")) {
                // TODO(on-keyday):
                // because JSONConverter not accept nullptr for array element
                // use range_exclusive(in syntax, `..`) for else cond
                // `..` match to any condition
                /*
                auto range = a.make<Range>();
                range.set_loc(l->loc);
                range->op = BinaryOp::range_exclusive;
                auto identity = a.make<Identity>();
                identity.set_loc(l->loc);
                identity->expr = range;
                cond.push_back(std::move(identity));
                */
                // if_->struct_union_type->exhaustive = true;
                auto body = a.make<BodyStatement>(l->loc);
                if_->blocks.push_back(body);
                body_with_struct(body, "to start `else` body");
            }
            else {
                if (cur_indent != 0) {
                    // 同じインデントだが elifでもelseでもなかったので読んだインデントを戻しておく
                    s.backward();
                }
            }

            // push_union_to_current_struct();

            return if_;
        }

        Node<Ident> parse_ident_no_scope(std::string_view hint) {
            if (state.error_tolerant) {
                auto f = s.consume_token(lexer::Tag::ident);
                if (!f) {
                    auto errs = s.token_error(lexer::Tag::ident, hint);
                    state.errors.locations.insert(state.errors.locations.end(), errs.locations.begin(), errs.locations.end());
                    s.recover_to_prev_skip();
                    auto ident = a.make<Ident>(s.loc(), "$dummy");  // error tolerant mode; return dummy ident
                    // ident->usage = IdentUsage::bad_ident;
                    return ident;
                }
                return a.make<Ident>(f->loc, std::string(s.text(*f)));
            }
            auto token = s.must_consume_token(lexer::Tag::ident, hint);
            return a.make<Ident>(token.loc, std::string(s.text(token)));
        }

        Node<Ident> parse_ident(std::string_view hint) {
            auto ident = parse_ident_no_scope(hint);
            // auto scope = state.current_scope();
            // scope->push(ident);
            // ident.ref(a)->scope = std::move(scope);
            return ident;
        }

        Node<Paren> parse_paren(lexer::LiteToken&& token) {
            auto paren = a.make<Paren>(token.loc);
            s.skip_white();
            paren->expr = parse_expr();
            s.skip_white();
            token = s.must_consume_token(")", "to close parenthesis");
            paren->end_loc = token.loc;
            return paren;
        }

        Node<StrLiteral> parse_str_literal(lexer::LiteToken&& lit) {
            auto literal = a.make<StrLiteral>(lit.loc);
            literal->value = std::string(s.text(lit));
            auto c = unescape(literal->value);
            if (!c) {
                s.report_error(lit.loc, "invalid string literal");
            }
            if (!futils::base64::encode(*c, literal->binary_value)) {
                s.report_error(lit.loc, "failed to encode string to base64 (internal error)");
            }
            return literal;
        }

        Node<RegexLiteral> parse_regex_literal(lexer::LiteToken&& lit) {
            auto literal = a.make<RegexLiteral>(lit.loc, NodeData<Literal>{}, std::string(s.text(lit)));
            return literal;
        }

        Node<TypeLiteral> parse_type_literal(lexer::LiteToken&& lit) {
            s.skip_line();
            auto typ = parse_type();
            s.skip_line();
            auto end_tok = s.must_consume_token(">", "to close type literal");
            auto literal = a.make<TypeLiteral>(lit.loc, NodeData<Literal>{}, std::move(typ));
            return literal;
        }

        Node<CharLiteral> parse_char_literal(lexer::LiteToken&& lit) {
            auto literal = a.make<CharLiteral>(lit.loc, NodeData<Literal>{}, std::string(s.text(lit)));
            auto c = unescape(literal->value);
            if (!c) {
                s.report_error(lit.loc, "invalid char literal");
            }
            std::u32string str;
            char32_t code;
            if (futils::utf::convert(*c, str)) {
                if (str.size() != 1) {
                    s.report_error(lit.loc, "invalid char literal; expect 1 char but got ", nums(str.size()));
                }
                code = str[0];
            }
            else {
                if (c->size() != 1) {
                    s.report_error(lit.loc, "invalid char literal; expect 1 char but got ", nums(c->size()));
                }
                code = (*c)[0];
            }
            literal->code = code;
            return literal;
        }

        /*
            <prim> ::= <int-literal> | <bool-literal> | <str-literal> | <ident> | "(" <expr> ")" | <if>
        */
        Node<Expr> parse_prim(bool* line_skipped) {
            if (auto token = s.consume_token(lexer::Tag::int_literal)) {
                return a.make<IntLiteral>(token->loc, NodeData<Literal>{}, std::string(s.text(*token)));
            }
            if (auto b = s.consume_token(lexer::Tag::bool_literal)) {
                return a.make<BoolLiteral>(b->loc, NodeData<Literal>{}, s.token_is(*b, "true"));
            }
            if (auto t = s.consume_token(lexer::Tag::str_literal)) {
                return parse_str_literal(std::move(*t));
            }
            if (auto t = s.consume_token(lexer::Tag::regex_literal)) {
                return parse_regex_literal(std::move(*t));
            }
            if (auto t = s.consume_token(lexer::Tag::char_literal)) {
                return parse_char_literal(std::move(*t));
            }
            if (auto i = s.consume_token("input")) {
                return a.make<SpecialLiteral>(i->loc, NodeData<Literal>{}, SpecialLiteralKind::input_);
            }
            if (auto o = s.consume_token("output")) {
                return a.make<SpecialLiteral>(o->loc, NodeData<Literal>{}, SpecialLiteralKind::output_);
            }
            if (auto c = s.consume_token("config")) {
                return a.make<SpecialLiteral>(c->loc, NodeData<Literal>{}, SpecialLiteralKind::config_);
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
                // text() は確保するので 1 回だけ。等値だけなら token_is で足りる。
                auto text = s.text(*i);
                auto i_desc = is_int_type(text);
                auto f_desc = is_float_type(text);
                if (i_desc || f_desc || text == "void" || text == "bool") {
                    Node<Type> type;
                    if (i_desc) {
                        type = a.make<IntType>(i->loc, NodeData<Type>{}, std::uint32_t(i_desc->bit_size), i_desc->is_signed, i_desc->endian);
                    }
                    else if (f_desc) {
                        type = a.make<FloatType>(i->loc, NodeData<Type>{}, std::uint32_t(f_desc->bit_size), f_desc->endian);
                    }
                    else if (text == "void") {
                        type = a.make<VoidType>(i->loc);
                    }
                    else if (text == "bool") {
                        type = a.make<BoolType>(i->loc);
                    }
                    else {
                        assert(false);
                    }
                    auto type_literal = a.make<TypeLiteral>(i->loc, NodeData<TypeLiteral>{}, std::move(type));
                    s.must_consume_token(lexer::Tag::ident, must_success);
                    return type_literal;
                }
            }
            else if (state.error_tolerant) {  // not found ident
                s.recover_to_prev_skip();
                auto err = s.token_error(lexer::Tag::ident, "field, variable, type name for type literal or function name expected");
                state.errors.locations.insert(state.errors.locations.end(), err.locations.begin(), err.locations.end());
                return a.make<BadExpr>(s.loc(), NodeData<Expr>{}, brgen::concat(state.errors.locations.back().msg));
            }
            auto ident = parse_ident("field, variable, type name for type literal or function name expected");
            return a.make<Reference>(ident.ref(a).loc(), NodeData<Expr>{}, std::move(ident));
        }

        void collect_comma(Node<Expr> args, std::vector<Node<Expr>>& res) {
            auto b = args.as<Binary>().ref(a);
            if (b && b->op == BinaryOp::comma) {
                collect_comma(b->left, res);
                collect_comma(b->right, res);
            }
            else {
                res.push_back(std::move(args));
            }
        }

        void collect_args(Node<Expr> args, std::vector<Node<Argument>>& res) {
            auto b = args.as<Binary>().ref(a);
            if (b && b->op == BinaryOp::comma) {
                collect_args(b->left, res);
                collect_args(b->right, res);
            }
            else if (b && b->op == BinaryOp::assign) {
                auto named_arg = a.make<NamedArgument>(b->left.ref(a).loc());
                named_arg->name = b->left;
                named_arg->value = b->right;
                res.push_back(named_arg);
            }
            else {
                auto arg = a.make<Argument>(args.ref(a).loc());
                arg->value = std::move(args);
                res.push_back(arg);
            }
        }

        Node<Call> parse_call(lexer::LiteToken&& token, Node<Expr>& p) {
            auto call = a.make<Call>(p.ref(a).loc(), NodeData<Expr>{}, p);
            // 引数が無くても Arguments は作る。f() でも end_loc を持たせるためと、
            // 参照側が毎回 null 検査をしなくて済むようにするため。
            auto args = a.make<Arguments>(token.loc);
            call->arguments = args;
            s.skip_white();
            if (!s.expect_token(")")) {
                auto raw = parse_expr();
                collect_args(raw, args->arguments);
                s.skip_white();
            }
            token = s.must_consume_token(")", "to close function call");
            args->end_loc = token.loc;
            return call;
        }

        Node<Expr> parse_call_or_cast(lexer::LiteToken&& token, Node<Expr>& p) {
            auto call = parse_call(std::move(token), p);
            if (auto typ = call.ref(a)->callee.as<TypeLiteral>().ref(a)) {
                return a.make<Cast>(call.ref(a).loc(), NodeData<Expr>{}, call, call.ref(a)->arguments);
            }
            // config.import("path") はパスが文字列リテラルなので parse で確定する。
            // 実際にファイルを読んで繋ぐのは別段の仕事なので、結果はここには置かず
            // side table (ImportResolution) 側に分ける。
            if (extract_name(call.ref(a)->callee) == "config.import") {
                auto arg = first_argument(call.ref(a)->arguments);
                if (arg.type() != NodeType::StrLiteral) {
                    s.report_error(call.ref(a).loc(), "config.import() requires a string literal path");
                }
                auto path = unescape(arg.as<StrLiteral>().ref(a)->value);
                if (!path) {
                    s.report_error(call.ref(a).loc(), "invalid string literal in config.import()");
                }
                auto import_ = a.make<Import>(call.ref(a).loc());
                import_->path = std::move(*path);
                return import_;
            }
            // available(x) / sizeof(x) / bit_sizeof(x) は名前の文字列一致だけで
            // 決まる。同名の fn を定義しても奪われないのは元の実装と同じ。
            auto name = callee_name(call.ref(a)->callee);
            if (name == "available" || name == "sizeof" || name == "bit_sizeof") {
                auto target = first_argument(call.ref(a)->arguments);
                if (!target) {
                    s.report_error(call.ref(a).loc(), name, "() requires at least one argument");
                }
                if (name == "available") {
                    auto avail = a.make<Available>(call.ref(a).loc());
                    avail->target = target;
                    // `available(x, u8)` の第 2 引数は型。分岐ごとに型が違う
                    // field で「今どちらか」を訊く形 (example/coap.bgn)。
                    // 型名は式の位置でも TypeLiteral に解けている。
                    if (auto want = nth_argument(call.ref(a)->arguments, 1)) {
                        auto lit = want.as<TypeLiteral>();
                        if (!lit) {
                            s.report_error(want.ref(a).loc(),
                                           "available()'s second argument must be a type");
                        }
                        avail->selected_type = lit;
                    }
                    return avail;
                }
                if (name == "bit_sizeof") {
                    // sizeof はバイト単位。ビット境界を跨ぐ field はそれでは
                    // 表せないので、ビット単位のほうを別に持つ。
                    auto bits = a.make<BitSizeof>(call.ref(a).loc());
                    bits->target = target;
                    return bits;
                }
                auto size = a.make<Sizeof>(call.ref(a).loc());
                size->target = target;
                return size;
            }
            return call;
        }

        Node<Index> parse_index(lexer::LiteToken&& token, Node<Expr>& p) {
            auto call = a.make<Index>(token.loc, NodeData<Expr>{}, std::move(p));
            s.skip_white();
            call->index = parse_expr();
            s.skip_white();
            token = s.must_consume_token("]", "to close index");
            call->end_loc = token.loc;
            return call;
        }

        Node<MemberAccess> parse_access(lexer::LiteToken&& token, Node<Expr> p) {
            s.skip_white();
            auto ident = parse_ident_no_scope("member ident expected after '.'");
            // ident.ref(a)->usage = IdentUsage::reference_member;
            auto member = a.make<MemberAccess>(token.loc, NodeData<Expr>{}, std::move(p), std::move(ident));
            // member->member.ref(a)->base = member;
            return member;
        }

        /*
            <post> ::= <prim>  ("(" <expr> ")" | <post> "." <ident> | <post> "[" <expr> "]")*
        */
        Node<Expr> parse_post(bool* line_skipped) {
            auto p = parse_prim(line_skipped);
            s.set_regex_mode(false);  // from here, regex literal is not allowed
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

        // 候補ごとに consume_token を呼ぶと、1 回ごとに
        // consume_token -> peek_token -> expect_token -> eos -> maybe_parse の
        // 連鎖が走る。ops は最大 15 個あり、example/ の実測ではこの比較が
        // 1252144 回、そのうち一致は 2.9%。
        // トークンは 1 度だけ取り出して、あとは本文比較だけを回す。
        std::optional<lexer::LiteToken> consume_op(size_t& i, auto& ops) {
            i = ops.size();
            if (s.eos()) {
                return std::nullopt;
            }
            auto tok = s.peek_token();
            for (std::size_t k = 0; k < ops.size(); k++) {
                bool hit;
                if constexpr (futils::helper::is_template_instance_of<std::decay_t<decltype(ops[k])>, std::pair>) {
                    hit = s.token_is(tok, ops[k].second);
                }
                else {
                    hit = s.token_is(tok, ops[k]);
                }
                if (hit) {
                    i = k;
                    s.consume();
                    return tok;
                }
            }
            return std::nullopt;
        }

        /*
            <unary> ::= <post> | <unary-op> <unary>
            <unary-op> ::= <unary-op> | <unary-op> <unary-op> <unary>
        */
        Node<Expr> parse_unary(bool* line_skipped) {
            std::vector<Ref<Unary>> stack;
            size_t i;
            s.skip_space();
            for (;;) {
                if (auto token = consume_op(i, nast::enum_array<UnaryOp>)) {
                    stack.push_back(a.make<Unary>(token->loc, NodeData<Expr>{}, UnaryOp(i)));
                    s.skip_white();
                    continue;
                }
                break;
            }
            auto target = parse_post(line_skipped);  // return non-nullptr or throw error
            while (stack.size()) {
                auto ptr = std::move(stack.back());
                stack.pop_back();
                ptr->target = std::move(target);
                target = std::move(ptr);
            }
            return target;
        }


        /*
        void check_duplicated_def(Ident* ident) {
            auto found = ident->scope->lookup_current(
                [&](Node<Ident>& i) {
                    if (i.ref(a)->usage != IdentUsage::unknown && i.ref(a)->name == ident->name) {
                        return true;
                    }
                    return false;
                },
                ident);
            if (found) {
                error(ident->loc, "duplicate definition of ", ident->name)
                    .error((*found)->loc, "previous definition is here")
                    .report();
            }
        }

        void rewrite_ident_scope(const Node<Ident>& ident) {
            std::erase_if(ident->scope->objects, [&](auto& i) {
                return i.lock() == ident;
            });
            ident->scope = state.current_scope();
            ident->scope->push(ident);
        }
        */

        void check_assignment(BinaryOp op, Node<Expr> left) {
            if (op == BinaryOp::define_assign ||
                op == BinaryOp::const_assign ||
                op == BinaryOp::in_assign) {
                auto ident = left.as<Reference>();
                if (!ident) {
                    if (state.error_tolerant) {
                        (void)state.errors.error(left.ref(a).loc(), "left of `:=`, `::=`, or `in` must be ident");
                        return;
                    }
                    s.report_error(left.ref(a).loc(), "left of `:=`, `::=`, or `in` must be ident");
                }
                /*
                ident->usage = assign->op == BinaryOp::const_assign
                                   ? IdentUsage::define_const
                                   : IdentUsage::define_variable;
                ident->base = assign;
                */
                // rewrite scope information for semantic analysis
                // rewrite_ident_scope((assign->left).as<Ident>());
                // check_duplicated_def(ident);
            }
            else {  // otherwise, assign
                Node<Reference> ident;
                if (!is_assignable(a, left, &ident)) {
                    if (state.error_tolerant) {
                        (void)state.errors.error(left.ref(a).loc(), "left of `=` must be ident, member access, indexed or input/output/config");
                        return;
                    }
                    s.report_error(left.ref(a).loc(), "left of `=` must be ident, member access, indexed or input/output/config");
                }
                if (ident) {
                    // rewrite_ident_scope(ident);
                }
            }
        }

        struct BinOpStack {
            size_t depth = 0;
            Node<Expr> expr;
        };

        bool appear_valid_range_end() {
            for (auto u : enum_array<UnaryOp>) {
                if (s.expect_token(u.second)) {
                    return true;
                }
            }
            if (s.expect_token(lexer::Tag::ident) ||
                s.expect_token(lexer::Tag::bool_literal) ||
                s.expect_token(lexer::Tag::int_literal) ||
                s.expect_token(lexer::Tag::str_literal) ||
                s.expect_token(lexer::Tag::char_literal) ||
                s.expect_token("input") || s.expect_token("output") ||
                s.expect_token("if") || s.expect_token("match")) {
                return true;
            }
            return false;
        }

        Node<Expr> parse_expr_identity(bool* line_skipped = nullptr) {
            // auto expr = parse_expr(line_skipped);
            // return a.make<Identity>(expr.ref(a).loc(), NodeData<Expr>{}, std::move(expr));
            return parse_expr(line_skipped);
        }

        /*
            <expr> ::= <unary> | <unary> <bin-op> <expr>
            <bin-op> ::= <bin-op> | <bin-op> <bin-op> <expr>
        */
        Node<Expr> parse_expr(bool* line_skipped = nullptr) {
            Node<Expr> expr;
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
                    if (op.expr.type() == NodeType::Binary) {
                        if (depth == bin_assign_layer) {
                            stack.push_back(std::move(op));
                        }
                        else {
                            auto b = op.expr.as<Binary>().ref(a);
                            b->right = std::move(expr);
                            expr = std::move(op.expr);
                        }
                    }
                    else if (op.expr.type() == NodeType::Range) {
                        auto b = op.expr.as<Range>().ref(a);
                        b->end = std::move(expr);
                        expr = std::move(op.expr);
                    }
                    else if (op.expr.type() == NodeType::Cond) {
                        auto cop = op.expr.as<Cond>().ref(a);
                        if (!cop->then) {
                            cop->then = std::move(expr);
                            s.skip_white();
                            auto token = s.must_consume_token(":", "to separate `then` and `else` part of conditional expression");
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
                if (depth == bin_cond_layer) {
                    while (stack_is_on_depth()) {
                        auto op = pop_stack();
                        auto cop = op.expr.as<Cond>().ref(a);
                        if (!cop->then || !cop->cond) {
                            s.report_error("expect cond with `cond` and `then` but not; parser bug");
                        }
                        cop->els = std::move(expr);
                        expr = std::move(op.expr);
                    }
                }
                else if (depth == bin_assign_layer) {
                    while (stack_is_on_depth()) {
                        auto op = pop_stack();
                        auto b = op.expr.as<Binary>().ref(a);
                        if (!b->left) {
                            s.report_error("expect binary with `left` but not; parser bug");
                        }
                        b->right = std::move(expr);
                        check_assignment(b->op, b->left);
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
            s.set_regex_mode(false);  // from here, regex literal is not allowed

            while (depth < bin_layer_len) {
                if (update_stack()) {
                    continue;
                }
                s.skip_space();
                if (depth == bin_cond_layer) {
                    if (auto token = s.consume_token("?")) {
                        s.set_regex_mode(true);  // from here, regex literal is allowed
                        stack.push_back(BinOpStack{.depth = depth, .expr = a.make<Cond>(token->loc, NodeData<Expr>{}, std::move(expr))});
                        s.skip_white();
                        parse_low();
                        continue;
                    }
                }
                else {
                    if (auto token = consume_op(i, bin_layers[depth])) {
                        s.set_regex_mode(true);  // from here, regex literal is allowed
                        if (depth == bin_compare_layer) {
                            if (auto bin = expr.as<Binary>().ref(a); bin && is_compare_op(bin->op)) {
                                // this is like `a == b == c` but this language not support it
                                s.report_error(token->loc, "unexpected `", s.text(*token), "`");
                            }
                        }
                        if (depth == bin_range_layer) {
                            if (auto bin = expr.as<Range>().ref(a); bin && is_range_op(bin->op)) {
                                // this is like `a .. b .. c` but this language not support it
                                s.report_error(token->loc, "unexpected `", s.text(*token), "`");
                            }
                            s.skip_space();  // for safety, skip only space, not line
                            auto r = a.make<Range>(token->loc, NodeData<Expr>{}, std::move(expr), Node<Expr>{}, *from_string<BinaryOp>(bin_layers[depth][i]));
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
                        auto b = a.make<Binary>(token->loc, NodeData<Expr>{}, *from_string<BinaryOp>(bin_layers[depth][i]), std::move(expr));
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
            s.set_regex_mode(true);
            return expr;
        }

        /*
            <loop> ::= "for" <expr>? (";" <expr>?)? (";" <expr>?)? <indent block>
        */
        Node<Statement> parse_for(lexer::LiteToken&& token) {
            // auto cs = state.cond_scope(for_->cond_scope, for_);
            s.skip_white();
            constexpr auto hint = "to start `for` body";
            if (s.expect_token(":")) {
                auto for_ = a.make<Loop>(token.loc);
                for_->body = parse_indent_block(for_, hint);
                return for_;
            }
            Node<Statement> init;
            if (!s.expect_token(";")) {
                init = parse_expr_like_statement(nullptr);
                s.skip_white();
                // like `for x in 0..10`
                if (auto in_ = s.consume_token("in")) {
                    auto range_loop = a.make<RangeLoop>(token.loc);
                    s.skip_white();
                    auto range = parse_expr();
                    check_assignment(BinaryOp::in_assign, init.as<Expr>());
                    // 束縛は := と同じ VariableDefinition (op だけ in_assign)。
                    // 宣言としての扱い (解決先・型の出し方) を共通にするため。
                    auto name = init.as<Reference>().ref(a)->name;
                    auto binding = a.make<VariableDefinition>(name.ref(a).loc());
                    binding->name = name;
                    binding->op = BinaryOp::in_assign;
                    binding->value = range;
                    range_loop->binding = binding;
                    s.skip_white();
                    range_loop->body = parse_indent_block(range_loop, hint);
                    return range_loop;
                }
            }
            auto for_ = a.make<Loop>(token.loc);
            for_->init = init;
            if (s.expect_token(":")) {
                // `for cond:` は init として読んだものがそのまま条件になる。
                // parse_expr_like_statement は真偽演算子を Assert に包むので剥がす
                // (元の parse.cpp:1108 は parse_expr で読むため包まれない)。
                if (auto asrt = init.as<Assert>()) {
                    for_->condition = asrt.ref(a)->expr;
                }
                else {
                    for_->condition = init.as<Expr>();
                }
                for_->body = parse_indent_block(for_, hint);
                return for_;
            }
            s.must_consume_token(";", " to separate `init` and `cond` part of `for` loop");
            s.skip_white();
            if (!s.expect_token(";")) {
                for_->condition = parse_expr();
                s.skip_white();
            }
            if (s.expect_token(":")) {
                for_->body = parse_indent_block(for_, hint);
                return for_;
            }
            s.must_consume_token(";", " to separate `cond` and `step` part of `for` loop");
            s.skip_white();
            if (!s.expect_token(":")) {
                for_->step = parse_expr_like_statement(nullptr);
                s.skip_white();
            }
            for_->body = parse_indent_block(for_, hint);
            return for_;
        }

        /*
            <func type> ::= "fn" "(" (<type> ("," <type>)*)? ")" ("->" <type>)?
        */
        // fn (a :int,b :int) -> int
        Node<FunctionType> parse_func_type(lexer::LiteToken&& tok) {
            auto func_type = a.make<FunctionType>(tok.loc, NodeData<Type>{.is_explicit = true});
            s.skip_white();
            s.must_consume_token("(", "to open function type parameter list");
            s.skip_white();
            bool second = false;
            while (!s.expect_token(")")) {
                if (second) {
                    s.must_consume_token(",", "to separate function type parameters");
                }
                if (s.consume_token(lexer::Tag::ident)) {
                    s.skip_white();
                }
                s.must_consume_token(":", "to separate function type parameter name and type");
                s.skip_white();
                auto tmp_push_3_ = parse_type();
                func_type->parameters.push_back(tmp_push_3_);
                s.skip_white();
                second = true;
            }
            s.must_consume_token(")", "to close function type parameter list");
            s.skip_space();  // for safety, skip only space, not line
            if (s.consume_token("->")) {
                s.skip_white();
                func_type->return_type = parse_type();
            }
            return func_type;
        }

        /*
            <type> ::= <array type> | <int type> | <str type> | <func type> | <ident type>
            <array type> ::= "[" <expr>? "]" <type>
            <int literal type> ::= <int literal>
            <str literal type> ::= <str literal>
            <ident type> ::= <ident> ("." <ident>)* ( "[" <type> "]" )?
        */
        Node<Type> parse_type() {
            if (auto arr_begin = s.consume_token("[")) {
                s.skip_white();
                Node<Expr> expr;
                if (!s.expect_token("]")) {
                    expr = parse_expr();
                    s.skip_white();
                }
                auto end_tok = s.must_consume_token("]", "to close array type");
                s.skip_space();
                auto base_type = parse_type();
                return a.make<ArrayType>(arr_begin->loc, NodeData<Type>{.is_explicit = true}, std::move(expr), end_tok.loc, std::move(base_type));
            }

            if (auto lit = s.consume_token(lexer::Tag::str_literal)) {
                return a.make<StrLiteralType>(lit->loc, NodeData<Type>{.is_explicit = true}, std::move(parse_str_literal(std::move(*lit))));
            }

            if (auto lit = s.consume_token(lexer::Tag::regex_literal)) {
                return a.make<RegexLiteralType>(lit->loc, NodeData<Type>{.is_explicit = true}, std::move(parse_regex_literal(std::move(*lit))));
            }

            if (auto fn = s.consume_token("fn")) {
                return parse_func_type(std::move(*fn));
            }

            if (auto void_ = s.consume_token("void")) {
                return a.make<VoidType>(void_->loc, NodeData<Type>{.is_explicit = true});
            }

            if (auto bool_ = s.consume_token("bool")) {
                return a.make<BoolType>(bool_->loc, NodeData<Type>{.is_explicit = true});
            }

            if (auto format = s.consume_token("format")) {
                auto fmt = parse_format(std::move(*format), true);
                // インライン format は名前で実体化しないので型パラメータを取れない。
                // スキーマ上も InlineStructType は Format しか持てない。
                auto concrete = fmt.as<Format>();
                if (!concrete) {
                    s.report_error(fmt.ref(a).loc(), "inline format cannot take type parameters");
                }
                return a.make<InlineStructType>(concrete.ref(a).loc(), NodeData<Type>{.is_explicit = true},
                                                std::move(concrete));
            }

            constexpr auto type_hint = "to specify type name, types are like `T`, `[]T`, `[x][10]T`, `fn(p :T,:U) -> T`, `\"magic_number\"`, `/regex/`, `imported.T`";

            // 型名の Ident。error tolerant の復旧では入力に無い名前を立てる。
            // 偽のトークンを作って後段に渡すのではなく、ここで Ident を作る。
            // トークンは入力の一部を指すものなので、入力に無い文字列は持てない。
            Ref<Ident> base;

            if (state.error_tolerant && !s.expect_token(lexer::Tag::ident)) {
                auto errs = s.token_error(lexer::Tag::ident, type_hint);
                state.errors.locations.insert(state.errors.locations.end(), errs.locations.begin(), errs.locations.end());
                base = a.make<Ident>(s.loc(), "$dummy");
                s.recover_to_prev_skip();
            }
            else {
                auto ident = s.must_consume_token(lexer::Tag::ident, type_hint);
                auto text = s.text(ident);
                if (auto desc = is_int_type(text)) {
                    return a.make<IntType>(ident.loc, NodeData<Type>{.is_explicit = true}, desc->bit_size, desc->is_signed, desc->endian);
                }
                if (auto desc = is_float_type(text)) {
                    return a.make<FloatType>(ident.loc, NodeData<Type>{.is_explicit = true}, desc->bit_size, desc->endian);
                }
                base = a.make<Ident>(ident.loc, std::string(text));
            }
            // base->usage = IdentUsage::maybe_type;
            // base->scope = state.current_scope();

            s.skip_space();

            Ref<WrapperType> type;
            // import type
            if (auto dot = s.consume_token(".")) {
                auto ref = a.make<Reference>(base.loc(), NodeData<Expr>{}, std::move(base));
                auto import_ref = parse_access(std::move(*dot), std::move(ref));
                auto imported_type = a.make<ImportedType>(import_ref.ref(a).loc(), NodeData<WrapperType>{NodeData<Type>{.is_explicit = true}}, std::move(import_ref));
                type = imported_type;
            }
            else {
                auto id = a.make<IdentType>(base.loc(), NodeData<WrapperType>{NodeData<Type>{.is_explicit = true}}, std::move(base));
                // id->import_ref = std::move(import_ref);
                type = id;
            }

            // generic type instantiation: `X[T, ...]`
            s.skip_space();
            if (auto lb = s.consume_token("[")) {
                auto generic = a.make<GenericType>(type.loc(), NodeData<Type>{.is_explicit = true});
                generic->base_type = type;
                for (;;) {
                    s.skip_white();
                    if (s.consume_token("]")) {
                        if (generic->type_arguments.empty()) {
                            if (state.error_tolerant) {
                                (void)state.errors.error(lb->loc, "empty type argument list");
                            }
                            else {
                                s.report_error(lb->loc, "empty type argument list");
                            }
                        }
                        break;
                    }
                    auto tmp_push_4_ = parse_type();
                    generic->type_arguments.push_back(tmp_push_4_);
                    s.skip_white();
                    if (s.expect_token("]")) {
                        continue;
                    }
                    s.must_consume_token(",", "to separate type arguments");
                }
                return generic;
            }

            return type;
        }

        /*
            <field type> ::= ":" <type> ("(" <expr> ")")?
        */
        // may returns expr if not field
        // top_level のとき、field の形の文は StateVariable になる。トップレベルの
        // field 宣言は線上に載る field ではなく、encode/decode に外から渡される
        // 実行時文脈 (state variable) の宣言で、位置だけで決まるので parse で
        // 分けられる。型を state / format 系に限る検査は解析段の仕事で、
        // ここではしない。
        Node<Statement> parse_field(Node<Statement> may_ident, bool top_level) {
            lexer::LiteToken token;
            Node<Ident> ident;
            if (may_ident) {
                if (may_ident.type() != NodeType::Reference) {
                    return may_ident;
                }
                s.skip_space();
                auto tmp = s.consume_token(":");
                if (!tmp) {
                    return may_ident;
                }
                ident = may_ident.as<Reference>().ref(a)->name;
                token = std::move(*tmp);
            }
            else {
                token = s.must_consume_token(":", "to specify field type");
            }

            // Field と StateVariable は同じ形 (name / type / arguments)。
            auto fill = [&](auto field) -> Node<Statement> {
                // field->colon_loc = token.loc;
                field->name = ident;
                s.skip_space();

                field->type = parse_type();

                /*
                if (field->name) {
                    field->name.ref(a)->expr_type = field->field_type;
                    field->name.ref(a)->base = field;
                    field->name.ref(a)->constant_level = ConstantLevel::variable;
                    if (!as_parameter) {  // as parameter, duplication check is delayed until all parameters are parsed, because of this case: `fn foo(x :int, x :int)`
                        field->name.ref(a)->usage = IdentUsage::define_field;
                        check_duplicated_def(field->name.ref(a).get());
                    }
                }
                */
                /*
                if (!as_parameter) {
                    field->belong = state.current_member();
                }
                */

                if (auto b = s.consume_token("(")) {
                    s.skip_white();

                    auto field_argument = a.make<Arguments>(b->loc);

                    if (!s.expect_token(")")) {
                        auto raw = parse_expr();
                        collect_args(raw, field_argument->arguments);
                        s.skip_white();
                    }

                    auto e = s.must_consume_token(")", "to close field argument");
                    field_argument->end_loc = e.loc;
                    field->arguments = field_argument;
                }

                /*
                if (!as_parameter) {
                    state.add_to_struct(field);
                }
                */

                s.skip_space();
                s.consume_token(lexer::Tag::comment);  // enforce parser to recognize comment after field definition

                /*
                if (auto comment = s.get_comments()) {
                    field->follow_comment = std::move(comment);
                }
                */
                return field;
            };
            auto loc = ident ? ident.ref(a).loc() : token.loc;
            if (top_level) {
                return fill(a.make<StateVariable>(loc));
            }
            return fill(a.make<Field>(loc));
        }

        void set_enum_value(Node<EnumMember> member, size_t& offset, NodeData<EnumMember>*& prev_specified) {
            if (!prev_specified) {
                auto int_lit = a.make<IntLiteral>(member.ref(a).loc(), NodeData<Literal>{}, "0");
                member.ref(a)->value = int_lit;
                prev_specified = member.ref(a).get();
            }
            else {
                // make (prev expr) + offset
                if (auto i_lit = prev_specified->value.as<IntLiteral>();
                    i_lit && i_lit.ref(a)->value == "0") {
                    member.ref(a)->value = a.make<IntLiteral>(member.ref(a).loc(), NodeData<Literal>{}, brgen::nums(offset));
                }
                else {
                    auto add = a.make<Binary>();
                    add.set_loc(member.ref(a).loc());
                    add->op = BinaryOp::add;
                    add->left = prev_specified->value;
                    add->right = a.make<IntLiteral>(member.ref(a).loc(), NodeData<Literal>{}, brgen::nums(offset));
                    member.ref(a)->value = add;
                }
            }
        }

        Node<EnumMember> parse_enum_member(const Node<Enum>& enum_, size_t& offset, NodeData<EnumMember>*& prev_specified) {
            auto ident = parse_ident("enum member ident expected");
            // ident.ref(a)->usage = IdentUsage::define_enum_member;
            // ident.ref(a)->expr_type = enum_.ref(a)->enum_type;
            // ident.ref(a)->constant_level = ConstantLevel::constant;
            // check_duplicated_def(ident.ref(a).get());
            auto member = a.make<EnumMember>(ident.ref(a).loc());
            member->name = ident;
            member->belong = enum_;
            // ident.ref(a)->base = member;
            s.skip_space();
            if (s.consume_token("=")) {
                s.skip_white();
                member->raw_expr = parse_expr();
                std::vector<Node<Expr>> commas;
                collect_comma(member->raw_expr, commas);
                if (commas.size() > 2) {
                    s.report_error(member->raw_expr.ref(a).loc(), "enum member value must be 1 or 2 elements but got ", nums(commas.size()));
                }
                for (auto& expr : commas) {
                    if (expr.as<StrLiteral>()) {
                        if (member->str_literal) {
                            s.report_error(expr.ref(a).loc(), "enum member str literal already specified");
                        }
                        member->str_literal = (expr).as<StrLiteral>();
                    }
                    else {
                        if (member->value) {
                            s.report_error(expr.ref(a).loc(), "enum member value already specified");
                        }
                        member->value = expr;
                        offset = 0;
                        prev_specified = member.get();
                    }
                }
                if (!member->value) {
                    set_enum_value(member.id(), offset, prev_specified);
                }
            }
            else {
                set_enum_value(member.id(), offset, prev_specified);
            }
            // member->comment = s.get_comments();
            enum_.ref(a)->members.push_back(member);
            s.skip_line();
            offset++;
            return member;
        }

        void parse_enum_base_type(Node<Enum>& enum_, lexer::LiteToken& base) {
            s.skip_white();
            enum_.ref(a)->base_type = parse_type();
            // enum_.ref(a)->enum_type.ref(a)->bit_size = enum_.ref(a)->base_type.ref(a)->bit_size;
            s.skip_space_comment();
            s.must_consume_token(lexer::Tag::line, "to separate enum base type");
            s.skip_line();
            auto indent = s.must_consume_token(lexer::Tag::indent, "to start enum member block");
            if (indent.loc.pos.len() != base.loc.pos.len()) {
                s.report_error(indent.loc, "indent size must be same as enum base");
            }
        }

        /*
            <enum> ::= "enum" <ident> ":\r\n" (":"<type>)? <enum member>+
            <enum member> ::= <indent> <ident> ("=" <expr>)?
         */
        Node<Enum> parse_enum(lexer::LiteToken&& token) {
            // set enum type
            auto enum_ = a.make<Enum>(token.loc);
            s.skip_white();
            enum_->name = parse_ident("enum name expected");
            // enum_->name.ref(a)->usage = IdentUsage::define_enum;
            // enum_->name.ref(a)->base = enum_;
            // check_duplicated_def(enum_->name.ref(a).get());
            enum_->enum_type = a.make<EnumType>(enum_.loc());
            enum_->enum_type.ref(a)->base = enum_;
            must_consume_indent_sign("to start enum member block");  // :<CR><LF>

            auto base = s.must_consume_token(lexer::Tag::indent, "to start enum member block");
            // auto m_scope = state.enter_member(enum_);
            // auto s_scope = state.new_indent(s, base.loc.pos.len(), enum_->scope, enum_);
            Node<Enum> enum_id = enum_;
            if (auto tok = s.consume_token(":")) {
                parse_enum_base_type(enum_id, base);
            }

            NodeData<EnumMember>* prev_specified = nullptr;
            size_t offset = 0;
            parse_enum_member(enum_id, offset, prev_specified);
            while (auto indent = s.peek_token(lexer::Tag::indent)) {
                if (indent->loc.pos.len() != base.loc.pos.len()) {
                    break;
                }
                s.must_consume_token(lexer::Tag::indent, "to continue enum member block");
                parse_enum_member(enum_id, offset, prev_specified);
            }
            // state.add_to_struct(enum_);
            return enum_;
        }

        /*
            <format> ::= "format" <ident> <indent block>
        */
        // 型パラメータを持つものは GenericFormat、持たないものは Format を返す。
        // 下流が Format だけを見れば具象だけが取れるように分けてある
        // (元は Format::type_parameters が空かどうかで毎回問うていた)。
        Node<NamedBodyStatement> parse_format(lexer::LiteToken&& token, bool allow_anonymous) {
            Node<Ident> format_name;
            s.skip_white();
            auto ident_parse = [&] {
                format_name = parse_ident("format name expected");
                // format_name.ref(a)->usage = IdentUsage::define_format;
                // check_duplicated_def(format_name.ref(a).get());
            };
            if (allow_anonymous) {
                if (!s.peek_token(":")) {
                    ident_parse();
                }
            }
            else {
                ident_parse();
            }
            // optional generic type parameter list: `format Foo[T, U]:`
            std::vector<Node<Ident>> type_param_idents;
            std::vector<Node<TypeParameter>> type_params;
            s.skip_space();
            if (s.consume_token("[")) {
                for (;;) {
                    s.skip_white();
                    if (auto t = s.consume_token("]")) {
                        break;
                    }
                    auto name = parse_ident_no_scope("type parameter name expected");
                    auto tp = a.make<TypeParameter>(name.ref(a).loc());
                    tp->name = name;
                    type_param_idents.push_back(name);
                    type_params.push_back(tp);
                    s.skip_white();
                    if (s.expect_token("]")) {
                        continue;
                    }
                    s.must_consume_token(",", "to separate type parameters");
                }
                s.skip_space();
            }
            Node<NamedBodyStatement> fmt;
            if (type_params.empty()) {
                auto concrete = a.make<Format>(token.loc);
                concrete->name = format_name;
                fmt = concrete;
            }
            else {
                auto generic = a.make<GenericFormat>(token.loc);
                generic->name = format_name;
                for (auto& tp : type_params) {
                    tp.ref(a)->belong = generic;
                }
                generic->type_parameters = std::move(type_params);
                fmt = generic;
            }
            {
                // auto m_scope = state.enter_member(fmt);
                fmt.ref(a)->body = parse_indent_block(fmt, "to start `format` body", &type_param_idents);
            }
            // because fmt->name->expr_type = fmt->body->struct_type
            // makes circular reference, so not use it
            // state.add_to_struct(fmt);

            // fetch encode_fn and decode_fn
            // auto enc = fmt->body.ref(a)->struct_type.ref(a)->lookup("encode");
            // if (auto fn = enc.as<Function>()) {
            // fmt->encode_fn = (enc).as<Function>();
            // }
            // auto dec = fmt->body.ref(a)->struct_type.ref(a)->lookup("decode");
            // if (auto fn = dec.as<Function>()) {
            // fmt->decode_fn = (dec).as<Function>();
            // }
            // lookup cast fn
            // fmt->body.ref(a)->struct_type.ref(a)->lookup([&](Node<NamedStatement>& m) {
            // if (auto fn = m.as<Function>(); fn && fn.ref(a)->parameters.size() == 0) {
            // if (auto i_ty = is_int_type(fn.ref(a)->name.ref(a)->name); i_ty) {
            // auto ok_ty = fn.ref(a)->return_type.as<IntType>();
            // if (ok_ty && ok_ty.ref(a)->is_signed == i_ty->is_signed && ok_ty.ref(a)->bit_size == i_ty->bit_size) {
            // fn.ref(a)->is_cast = true;
            // fn.ref(a)->name.ref(a)->usage = IdentUsage::define_cast_fn;
            // fmt->cast_fns.push_back((m).as<Function>());
            // }
            // }
            // if (auto f_ty = is_float_type(fn.ref(a)->name.ref(a)->name); f_ty) {
            // auto ok_ty = fn.ref(a)->return_type.as<FloatType>();
            // if (ok_ty && ok_ty.ref(a)->bit_size == f_ty->bit_size) {
            // fn.ref(a)->is_cast = true;
            // fn.ref(a)->name.ref(a)->usage = IdentUsage::define_cast_fn;
            // fmt->cast_fns.push_back((m).as<Function>());
            // }
            // }
            // if (fn.ref(a)->name.ref(a)->name == "bool") {
            // auto ok_ty = fn.ref(a)->return_type.as<BoolType>();
            // if (ok_ty) {
            // fn.ref(a)->is_cast = true;
            // fn.ref(a)->name.ref(a)->usage = IdentUsage::define_cast_fn;
            // fmt->cast_fns.push_back((m).as<Function>());
            // }
            // }
            // }
            // return false;
            // });

            return fmt;
        }

        /*
            <state> ::= "state" <ident> <indent block>
        */
        Node<State> parse_state(lexer::LiteToken&& token) {
            auto state_ = a.make<State>(token.loc);
            s.skip_white();

            state_->name = parse_ident("state name expected");
            // state_->name.ref(a)->usage = IdentUsage::define_state;
            // state_->name.ref(a)->base = state_;
            // check_duplicated_def(state_->name.ref(a).get());
            {
                // auto m_scope = state.enter_member(state_);
                state_->body = parse_indent_block(state_, "to start `state` body");
            }
            // `state_->name->expr_type = state_->body->struct_type`
            // makes circular reference, so not use it
            // state.add_to_struct(state_);

            return state_;
        }

        /*
            <fn> ::= "fn" <ident> "(" (<ident> : <type> ("," <ident > <type>)*)? ")" ("->" <type>)? <indent block>
        */
        Node<Function> parse_fn(lexer::LiteToken&& token) {
            auto fn = a.make<Function>(token.loc);
            s.skip_white();
            fn->name = parse_ident("function name expected");
            // fn->name.ref(a)->usage = IdentUsage::define_fn;
            // fn->name.ref(a)->base = fn;
            // check_duplicated_def(fn->name.ref(a).get());
            // fn->name.ref(a)->constant_level = ConstantLevel::constant;
            // fn->belong = state.current_member();
            // fn->func_type = a.make<FunctionType>(fn.loc());
            // fn->name.ref(a)->expr_type = fn->func_type;
            s.skip_white();
            lexer::Loc end_loc;
            std::vector<Node<Ident>> ident_param;
            auto b = s.must_consume_token("(", "to open function parameter list");
            for (;;) {
                s.skip_white();
                if (auto t = s.consume_token(")")) {
                    end_loc = t->loc;
                    break;
                }
                Node<Ident> ident;
                if (!s.expect_token(":")) {
                    ident = parse_ident_no_scope("to specify function parameter name");
                    // ident.ref(a)->usage = IdentUsage::define_arg;
                    // ident->scope = state.current_scope();
                    ident_param.push_back(ident);
                    s.skip_white();
                }
                s.must_consume_token(":", "to separate function parameter name and type");
                s.skip_white();

                auto type = parse_type();
                auto param = a.make<Parameter>(ident ? ident.ref(a).loc() : type.ref(a).loc());
                param->name = ident;
                param->type = type;
                fn->parameters.push_back(param);
                // auto f = parse_field(std::move(ident), true);
                // auto field = (f).as<Field>();
                // field->belong = fn;
                // fn->parameters.push_back(field);
                // fn->func_type->parameters.push_back(field->field_type);
                if (s.expect_token(")") || s.consume_token(",")) {
                    continue;
                }
                // this must fail
                s.must_consume_token(",", "to separate function parameters. maybe you forget `)` to close function parameter list");
            }
            s.skip_white();
            if (auto r = s.consume_token("->")) {
                s.skip_white();
                fn->return_type = parse_type();
            }
            else {
                fn->return_type = a.make<VoidType>(end_loc);
            }

            // fn->func_type->return_type = fn->return_type;

            // state.add_to_struct(fn);

            {
                // auto m_scope = state.enter_member(fn);
                // auto typ = state.enter_struct(fn->struct_type);
                fn->body = parse_indent_block(fn, "to start `fn` body", &ident_param);
            }

            return fn;
        }

        Node<Loop> lookup_loop_scope() {
            // auto scope = state.current_scope();
            // for (auto s = scope; s; s = s->prev.lock()) {
            //     if (auto loop = s->owner.lock(); loop.as<Loop>()) {
            //         return (loop).as<Loop>();
            //     }
            // }
            return nullref;
        }

        Node<Function> lookup_function_scope() {
            // auto scope = state.current_scope();
            // for (auto s = scope; s; s = s->prev.lock()) {
            //     if (auto fn = s->owner.lock(); fn.as<Function>()) {
            //         return (fn).as<Function>();
            //     }
            // }
            return nullref;
        }

        // 原文に書かれた名前の並びを取り出す。input.endian や config.url のように
        // SpecialLiteral から始まる MemberAccess の連鎖だけを見る。
        // 識別子が何を指すかは問わないので、解決の前でも決まる。
        std::string extract_name(const Node<Expr>& expr) {
            if (expr.type() == NodeType::SpecialLiteral) {
                return to_string(expr.as<SpecialLiteral>().ref(a)->kind, 1);
            }
            if (expr.type() == NodeType::MemberAccess) {
                auto access = expr.as<MemberAccess>().ref(a);
                auto base = extract_name(access->base);
                if (base.empty()) {
                    return {};
                }
                return base + "." + access->member.ref(a)->identifier;
            }
            return {};
        }

        // 呼び出し先が素の識別子ならその名前。error / available / sizeof の判定に使う。
        std::string callee_name(const Node<Expr>& expr) {
            if (expr.type() != NodeType::Reference) {
                return {};
            }
            return expr.as<Reference>().ref(a)->name.ref(a)->identifier;
        }

        Node<Expr> nth_argument(const Node<Arguments>& args, std::size_t i) {
            auto list = args.ref(a);
            if (!list || list->arguments.size() <= i) {
                return nullref;
            }
            return list->arguments[i].ref(a)->value;
        }

        Node<Expr> first_argument(const Node<Arguments>& args) {
            return nth_argument(args, 0);
        }

        bool is_order_name(std::string_view name) {
            return name == "input.endian" || name == "input.bit_order" ||
                   name == "input.bit_order.stream" || name == "input.bit_order.mapping";
        }

        // 文の位置に来た組み込みの書き換え。名前の並びと形だけで決まるものに限る。
        // input.* / config.* は同じ構文空間を共有するので、判定の順序が意味を持つ。
        Node<Statement> rewrite_builtin_statement(const Node<Statement>& node) {
            if (node.type() == NodeType::Call) {
                auto call = node.as<Call>();
                auto args = call.ref(a)->arguments;
                if (callee_name(call.field<"callee">(a)) == "error") {
                    auto first = first_argument(args);
                    if (first.type() != NodeType::StrLiteral) {
                        s.report_error(call.ref(a).loc(), "error() requires a string literal as the first argument");
                    }
                    auto err = a.make<ExplicitError>(call.ref(a).loc());
                    err->message = first.as<StrLiteral>();
                    err->extra_arguments = args;
                    return err;
                }
                auto name = extract_name(call.ref(a)->callee);
                if (name.starts_with("config.")) {
                    auto meta = a.make<Metadata>(call.ref(a).loc());
                    meta->name = name;
                    meta->arguments = args;
                    return meta;
                }
                return nullref;
            }
            auto bin = node.as<Binary>().ref(a);
            if (!bin || bin->op != BinaryOp::assign) {
                return nullref;
            }
            auto name = extract_name(bin->left);
            if (name.empty()) {
                return nullref;
            }
            if (is_order_name(name)) {
                auto order = a.make<SpecifyOrder>(bin.loc());
                // どの指定か (input.endian / input.bit_order / ...) を落とすと
                // 逆変換 (unparse) できなくなるので名前ごと持つ。
                order->name = std::move(name);
                order->order = bin->right;
                return order;
            }
            if (name.starts_with("config.")) {
                auto arg = a.make<Argument>(bin->right.ref(a).loc());
                arg->value = bin->right;
                auto args = a.make<Arguments>(bin->right.ref(a).loc());
                args->arguments.push_back(arg);
                auto meta = a.make<Metadata>(bin.loc());
                meta->name = name;
                meta->arguments = args;
                return meta;
            }
            return nullref;
        }

        Node<Statement> parse_expr_like_statement(bool* prev_skip_line) {
            Node<Statement> node = parse_expr(prev_skip_line);
            if (auto rewritten = rewrite_builtin_statement(node); rewritten) {
                return rewritten;
            }
            if (auto bin = node.as<Binary>().ref(a); bin) {
                if (is_boolean_op(bin->op)) {
                    auto assert = a.make<Assert>(bin.loc());
                    assert->expr = bin;
                    node = assert;
                }
                // is_assign_op の範囲は define_assign / const_assign を含むので、
                // 定義側を先に見ないと VariableDefinition に到達しない。
                else if (is_define_op(bin->op)) {
                    auto var_def = a.make<VariableDefinition>(bin.loc());
                    auto ref = bin->left.as<Reference>().ref(a)->name;
                    var_def->name = ref;
                    var_def->value = bin->right;
                    var_def->op = bin->op;
                    node = var_def;
                }
                else if (is_assign_op(bin->op)) {
                    auto assign = a.make<Assign>(bin.loc());
                    assign->assignee = bin->left;
                    assign->value = bin->right;
                    assign->op = bin->op;
                    node = assign;
                }
            }
            return node;
        }

        /*
            <statement> ::= <for> | <if> | <format> | <fn> | <return> | <break> | <continue> | <expr or field>
            <expr or field> ::= <expr>? <field type>
        */
        // top_level はモジュール直下だけ真。field の形をした文が state variable に
        // なるかどうかは位置で決まる。
        Node<Statement> parse_statement(bool* prev_skip_line = nullptr, bool top_level = false) {
            auto set_skip = [&] {
                if (prev_skip_line) {
                    *prev_skip_line = true;
                }
            };
            auto skip_last = [&] {
                s.skip_space_comment();

                if (!s.eos() && s.expect_token(lexer::Tag::line)) {
                    s.must_consume_token(lexer::Tag::line, must_success);
                    s.skip_line();
                    set_skip();
                }
                else if (auto prev = s.prev_token(); prev && prev->tag == lexer::Tag::line) {
                    // The statement's terminating newline was already consumed by a
                    // nested indent block (e.g. a field whose type is an inline
                    // `format:` that is the last member of its struct). We are now at
                    // the start of a fresh physical line, so the logical line is
                    // complete. Without marking it skipped, parse_a_line would treat
                    // the following column-0 definition (which has no `indent` token)
                    // as a continuation of this line and misparse it as a nested
                    // member. See example/thrift_compact_protocol.bgn.
                    set_skip();
                }
            };

            if (auto loop = s.consume_token("for")) {
                set_skip();
                return parse_for(std::move(*loop));
            }

            if (auto format = s.consume_token("format")) {
                set_skip();
                return parse_format(std::move(*format), false);
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
                auto ret_ = a.make<Return>(ret->loc);
                s.skip_space();
                if (!s.eos() && !s.consume_token(lexer::Tag::line)) {
                    ret_->expr = parse_expr(prev_skip_line);
                }
                skip_last();
                ret_->related_function = lookup_function_scope();
                return ret_;
            }

            if (auto br = s.consume_token("break")) {
                skip_last();
                auto brk = a.make<Break>(br->loc);
                brk->related_loop = lookup_loop_scope();
                return brk;
            }

            if (auto cont = s.consume_token("continue")) {
                skip_last();
                auto con = a.make<Continue>(cont->loc);
                con->related_loop = lookup_loop_scope();
                return con;
            }

            Node<Statement> node;
            if (s.expect_token(":")) {
                node = parse_field(nullref, top_level);
            }
            else {
                auto expr = parse_expr_like_statement(prev_skip_line);
                node = parse_field(expr, top_level);
            }

            skip_last();

            return node;
        }
    };

    Node<Module> parse(Arena& a, Stream& stream, LocationError* err_or_warn, ParseOption option) {
        LocationError ignore;
        if (!err_or_warn) {
            err_or_warn = &ignore;
        }
        Parser p(stream, *err_or_warn, a);
        stream.set_collect_comments(option.collect_comments);
        p.state.error_tolerant = option.error_tolerant;
        return p.parse();
    }
}  // namespace brgen::nast
