/*license*/
#pragma once
#include "../lexer/token.h"
#include "../lexer/lexer.h"
#include <list>
#include <optional>
#include <helper/defer.h>
#include <code/src_location.h>
#include <map>
#include "node/ast.h"
#include "../common/stack.h"
#include "../common/file.h"

namespace brgen::ast {

    struct ContextInfo;

    struct Stream {
       private:
        std::list<lexer::Token> tokens;
        using iterator = typename std::list<lexer::Token>::iterator;
        iterator cur;
        ContextInfo* info = nullptr;
        File* input;

        [[noreturn]] void report_error(std::string&& msg, lexer::Loc pos) {
            error(pos, msg).report();
        }

        Stream() = default;
        friend struct Context;

        void maybe_parse() {
            if (cur == tokens.end()) {
                auto token = input->parse();
                if (!token) {
                    return;
                }
                if (token->tag == lexer::Tag::error) {
                    report_error(std::move(token->token), token->loc);
                }
                tokens.push_back(std::move(*token));
                cur = std::prev(tokens.end());
            }
        }

        lexer::Loc last_loc() {
            if (eos()) {
                auto copy = cur;
                copy--;
                return {lexer::Pos{copy->loc.pos.end, copy->loc.pos.end + 1}, copy->loc.file};
            }
            else {
                return cur->loc;
            }
        }

       public:
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
        [[noreturn]] void token_expect_error(auto&& expected, auto&& found) {
            lexer::Pos pos;
            std::string buf;
            appends(buf, "expect token ", expected, " but found ");
            if (eos()) {
                append(buf, "<EOF>");
                auto copy = cur;
                copy--;
                pos = {copy->loc.pos.end, copy->loc.pos.end + 1};
            }
            else {
                append(buf, found(cur));
                pos = cur->loc.pos;
            }
            report_error(std::move(buf), last_loc());
        }

       public:
        lexer::Token must_consume_token(std ::string_view view) {
            auto f = consume_token(view);
            if (!f) {
                token_expect_error(view, [](auto& cur) { return cur->token; });
            }
            return *f;
        }

        lexer::Token must_consume_token(lexer::Tag tag) {
            auto f = consume_token(tag);
            if (!f) {
                token_expect_error(lexer::tag_str[int(tag)], [](auto& cur) { return lexer::tag_str[int(cur->tag)]; });
            }
            return *f;
        }

        void skip_tag(auto... t) {
            while (!eos()) {
                if ((... || expect_token(t))) {
                    consume();
                    continue;
                }
                break;
            }
        }

        // Tag::space
        void skip_space() {
            skip_tag(lexer::Tag::space);
        }

        // Tag::space, Tag::line
        void skip_line() {
            skip_tag(lexer::Tag::space, lexer::Tag::line);
        }

        // Tag::space, Tag::line, Tag::indent
        void skip_white() {
            skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::indent);
        }

        ContextInfo* context() const {
            return info;
        }

       private:
        auto enter_stream(auto&& fn) -> result<std::invoke_result_t<decltype(fn)>> {
            try {
                cur = tokens.begin();
                return fn();
            } catch (LocationError& err) {
                return unexpect(std::move(err));
            }
        }
    };

    using defstack = Stack<Definitions>;

    struct ContextInfo {
       private:
        Stream* s = nullptr;
        size_t indent = 0;
        defstack stack;
        std::shared_ptr<Format> current_fmt_;

       public:
        auto new_indent(size_t new_, std::shared_ptr<StackFrame<Definitions>>& frame) {
            if (indent >= new_) {
                s->report_error("expect larger indent but not");
            }
            auto old = std::exchange(indent, std::move(new_));
            stack.enter_branch();
            frame = stack.current_frame();
            return utils::helper::defer([=, this] {
                indent = std::move(old);
                stack.leave_branch();
            });
        }

        auto enter_fmt(const std::shared_ptr<Format>& f) {
            f->belong = current_fmt_;
            current_fmt_ = f;
            return utils::helper::defer([this] {
                current_fmt_ = current_fmt_->belong.lock();
            });
        }

        std::shared_ptr<Format> current_fmt() {
            return current_fmt_;
        }

        size_t current_indent() {
            return indent;
        }

        define_frame reset_stack() {
            stack = {};
            return stack.current_frame();
        }

        define_frame current_definitions() {
            return stack.current_frame();
        }
    };

    struct Context {
       private:
        Stream stream;
        ContextInfo info;

       public:
        auto enter_stream(File* file, auto&& fn) {
            stream.info = &info;
            return stream.enter_stream([&] {
                stream.input = file;
                return fn(stream);
            });
        }
    };

}  // namespace brgen::ast
