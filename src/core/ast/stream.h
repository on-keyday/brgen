/*license*/
#pragma once
#include "../lexer/token.h"
#include "../lexer/lexer.h"
#include <list>
#include <optional>
#include <helper/defer.h>
#include <code/src_location.h>
#include <map>
#include "ast.h"
#include "../common/file.h"

namespace brgen::ast {

    struct Stream {
       private:
        std::list<lexer::Token> tokens;
        using iterator = typename std::list<lexer::Token>::iterator;
        iterator cur;
        File* input;
        size_t line = 1;
        size_t col = 1;
        std::vector<std::shared_ptr<Comment>> comments;
        bool collect_comments = false;
        lexer::Option lex_option;

        Stream() = default;
        friend struct Context;

        void maybe_parse() {
            if (cur == tokens.end()) {
                auto token = input->parse(lex_option);
                if (!token) {
                    return;
                }
                if (token->tag == lexer::Tag::error) {
                    token->loc.line = line;
                    token->loc.col = col;
                    error(token->loc, std::move(token->token)).report();
                }
                token->loc.line = line;
                token->loc.col = col;
                col += token->loc.pos.len();
                if (token->tag == lexer::Tag::line) {
                    line++;
                    col = 1;
                }
                if (collect_comments && token->tag == lexer::Tag::comment) {
                    comments.push_back(std::make_shared<Comment>(token->loc, token->token));
                }
                tokens.push_back(std::move(*token));
                cur = std::prev(tokens.end());
            }
        }

        lexer::Loc last_loc() {
            if (eos()) {
                if (cur == tokens.begin()) {
                    return lexer::Loc{lexer::Pos{0, 0}, input->index(), 1, 1};
                }
                auto copy = cur;
                copy--;
                return {lexer::Pos{copy->loc.pos.end, copy->loc.pos.end + 1}, copy->loc.file, copy->loc.line, copy->loc.col};
            }
            else {
                return cur->loc;
            }
        }

       public:
        // discard tokens before cur
        void shrink() {
            tokens.erase(tokens.begin(), cur);
            cur = tokens.begin();
        }

        std::list<lexer::Token> take() {
            auto copy = std::move(tokens);
            tokens.clear();
            cur = tokens.begin();
            return copy;
        }

        [[noreturn]] void report_error(auto&&... data) {
            error(last_loc(), "parser error: ", data...).report();
        }

        [[noreturn]] void report_error(lexer::Loc loc, auto&&... data) {
            error(loc, "parser error: ", data...).report();
        }

        // end of stream
        bool eos() {
            maybe_parse();
            return cur == tokens.end();
        }

        void consume() {
            if (eos()) {
                return;
            }
            cur++;
        }

        lexer::Loc loc() {
            maybe_parse();
            if (eos()) {
                return last_loc();
            }
            return cur->loc;
        }

        bool expect_token(lexer::Tag tag) {
            if (eos()) {
                return false;
            }
            return cur->tag == tag;
        }

        bool expect_token(std::string_view s) {
            if (eos()) {
                return false;
            }
            return cur->token == s;
        }

        std::optional<lexer::Token> peek_token(std::string_view s) {
            if (!expect_token(s)) {
                return std::nullopt;
            }
            // only copy for fallback
            return *cur;
        }

        std::optional<lexer::Token> peek_token(lexer::Tag t) {
            if (!expect_token(t)) {
                return std::nullopt;
            }
            // only copy for fallback
            return *cur;
        }

        lexer::Token peek_token() {
            return *cur;
        }

        std::optional<lexer::Token> consume_token(std::string_view s) {
            if (auto token = peek_token(s)) {
                consume();
                return token;
            }
            return std::nullopt;
        }

        std::optional<lexer::Token> consume_token(lexer::Tag t) {
            if (auto token = peek_token(t)) {
                consume();
                return token;
            }
            return std::nullopt;
        }

       private:
        [[nodiscard]] auto token_expect_error(auto&& expected, auto&& found) {
            std::string buf;
            appends(buf, "expect token ", expected, " but found ");
            if (eos()) {
                append(buf, "<EOF>");
            }
            else {
                append(buf, found(cur));
            }
            return error(last_loc(), std::move(buf));
        }

       public:
        auto token_error(lexer::Tag tag) {
            return token_expect_error(lexer::to_string(tag), [](auto& cur) { return lexer::enum_array<lexer::Tag>[int(cur->tag)].second; });
        }

        auto token_error(std::string_view s) {
            return token_expect_error(s, [](auto& cur) { return cur->token; });
        }

        lexer::Token must_consume_token(std ::string_view view) {
            auto f = consume_token(view);
            if (!f) {
                token_error(view).report();
            }
            return *f;
        }

        lexer::Token must_consume_token(lexer::Tag tag) {
            auto f = consume_token(tag);
            if (!f) {
                token_error(tag).report();
            }
            return *f;
        }

       private:
        void skip_tag(auto... t) {
            while (!eos()) {
                if ((... || expect_token(t))) {
                    consume();
                    continue;
                }
                break;
            }
        }

       public:
        // Tag::space, Tag::comment
        void skip_space() {
            skip_tag(lexer::Tag::space, lexer::Tag::comment);
        }

        // Tag::space, Tag::line, Tag::comment
        void skip_line() {
            skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::comment);
        }

        // Tag::space, Tag::line, Tag::indent, Tag::comment
        void skip_white() {
            skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::indent, lexer::Tag::comment);
        }

        std::shared_ptr<Node> get_comments() {
            if (comments.size() == 0) {
                return nullptr;
            }
            if (comments.size() == 1) {
                auto c = std::move(comments[0]);
                comments.clear();
                return c;
            }
            auto c = std::make_shared<CommentGroup>(comments[0]->loc, std::move(comments));
            comments.clear();
            return c;
        }

        void backward() {
            if (cur == tokens.begin()) {
                return;
            }
            cur--;
        }

        std::optional<lexer::Token> prev_token() {
            if (cur == tokens.begin()) {
                return std::nullopt;
            }
            return *std::prev(cur);
        }

        void set_collect_comments(bool b) {
            collect_comments = b;
        }

        void set_regex_mode(bool b) {
            lex_option.regex_mode = b;
        }

       private:
        auto enter_stream(auto&& fn) -> result<std::invoke_result_t<decltype(fn), Stream&>> {
            try {
                cur = tokens.begin();
                return fn(*this);
            } catch (LocationError& err) {
                return unexpect(std::move(err));
            }
        }
    };

    struct Context {
       private:
        Stream s;

       public:
        auto enter_stream(File* file, auto&& parser) {
            s.input = file;
            return s.enter_stream(parser);
        }
    };

}  // namespace brgen::ast
