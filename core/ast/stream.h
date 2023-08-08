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
#include "stack.h"

namespace ast {

    struct ContextInfo;

    struct StreamError {
        std::string msg;
        lexer::FileIndex file;
        utils::code::SrcLoc loc;
        std::string src;

        std::string to_string(std::string_view file) {
            std::string buf;

            appends(buf, "error: ", msg, "\n",
                    file, ":", nums(loc.line + 1), ":", nums(loc.pos + 1), ":\n",
                    src);
            return buf;
        }
    };

    struct Stream {
       private:
        std::list<lexer::Token> tokens;
        using iterator = typename std::list<lexer::Token>::iterator;
        iterator cur;
        void* seq_ptr = nullptr;
        std::uint64_t cur_file = 0;
        ContextInfo* info = nullptr;
        std::optional<lexer::Token> (*parse)(void* seq, std::uint64_t file) = nullptr;
        std::pair<std::string, utils::code::SrcLoc> (*dump)(void* seq, lexer::Pos pos) = nullptr;

        template <class T>
        static std::optional<lexer::Token> do_parse(void* ptr, std::uint64_t file) {
            return lexer::parse_one(*static_cast<utils::Sequencer<T>*>(ptr), file);
        }

        template <class T>
        static std::pair<std::string, utils::code::SrcLoc> dump_source(void* ptr, lexer::Pos pos) {
            auto& seq = *static_cast<utils::Sequencer<T>*>(ptr);
            seq.rptr = pos.begin;
            std::string out;
            auto loc = utils::code::write_src_loc(out, seq, pos.len());
            return {out, loc};
        }

        std::optional<lexer::Token> call_parse() {
            if (parse) {
                return parse(seq_ptr, cur_file);
            }
            return std::nullopt;
        }

        [[noreturn]] void report_error(lexer::Token& token) {
            auto text = dump(seq_ptr, token.loc.pos);
            throw StreamError{std::move(token.token), token.loc.file, text.second, std::move(text.first)};
        }

        Stream() = default;
        friend struct Context;

        void maybe_parse() {
            if (cur == tokens.end()) {
                auto token = call_parse();
                if (!token) {
                    return;
                }
                if (token->tag == lexer::Tag::error) {
                    report_error(*token);
                }
                tokens.push_back(std::move(*token));
                cur = std::prev(tokens.end());
            }
        }

       public:
        [[noreturn]] void report_error(auto&&... data) {
            lexer::Token token;
            token.tag = lexer::Tag::error;
            if (eos()) {
                auto copy = cur;
                copy--;
                token.loc.pos = {copy->loc.pos.end, copy->loc.pos.end + 1};
            }
            else {
                token.loc = cur->loc;
            }
            appends(token.token, "parser error:", data...);
            report_error(token);
        }

        template <class T>
        [[nodiscard]] auto set_seq(utils::Sequencer<T>& seq, std::uint64_t file) {
            auto old_parse = parse;
            auto old_dump = dump;
            auto old_ptr = seq_ptr;
            auto old_file = cur_file;

            seq_ptr = std::addressof(seq);
            parse = do_parse<T>;
            dump = dump_source<T>;
            cur_file = file;
            return utils::helper::defer([=, this] {
                cur_file = old_file;
                seq_ptr = old_ptr;
                parse = old_parse;
                dump = old_dump;
            });
        }

        void add_token(auto&& input, lexer::FileIndex file = lexer::builtin) {
            auto seq = utils::make_ref_seq(input);
            auto s = set_seq(seq, file);
            std::list<lexer::Token> tmp;
            for (auto input = call_parse(); input; input = call_parse()) {
                if (input->tag == lexer::Tag::error) {
                    report_error(*input);
                }
                tmp.push_back(std::move(*input));
            }
            if (cur == tokens.begin()) {
                tokens.splice(cur, std::move(tmp));
                cur = tokens.begin();
            }
            else {
                auto prev = cur;
                prev--;
                tokens.splice(cur, std::move(tmp));
                prev++;
                cur = prev;
            }
        }

        auto fallback() {
            maybe_parse();
            auto ptr = cur;
            iterator prev;
            bool was_begin = ptr == tokens.begin();
            bool was_end = ptr == tokens.end();
            if (!was_begin && was_end) {
                prev = std::prev(ptr);
            }
            return utils::helper::defer([=, this] {
                if (was_begin) {
                    cur = tokens.begin();
                }
                else if (was_end) {
                    auto cp = prev;
                    cur = ++cp;
                }
                else {
                    cur = ptr;
                }
            });
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
            lexer::Token token;
            token.tag = lexer::Tag::error;
            appends(token.token, "expect token ", expected, " but found ");
            if (eos()) {
                append(token.token, "<EOF>");
                auto copy = cur;
                copy--;
                token.loc.pos = {copy->loc.pos.end, copy->loc.pos.end + 1};
                token.loc.file = cur_file;
            }
            else {
                append(token.token, found(cur));
                token.loc = cur->loc;
            }
            report_error(token);
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
        std::optional<StreamError> enter_stream(auto&& fn) {
            try {
                cur = tokens.begin();
                fn();
            } catch (StreamError& err) {
                return err;
            }
            return std::nullopt;
        }
    };

    using defstack = Stack<Definitions>;

    struct ContextInfo {
       private:
        Stream* s = nullptr;
        size_t indent = 0;
        defstack stack;

       public:
        auto new_indent(size_t new_, std::shared_ptr<StackFrame<Definitions>>& frame) {
            if (indent >= new_) {
                s->report_error("expect largeer indent but not");
            }
            auto old = std::exchange(indent, std::move(new_));
            stack.enter_branch();
            frame = stack.current_frame();
            return utils::helper::defer([=, this] {
                indent = std::move(old);
                stack.leave_branch();
            });
        }

        size_t current_indent() {
            return indent;
        }

        defframe reset_stack() {
            stack = {};
            return stack.current_frame();
        }

        defframe current_definitions() {
            return stack.current_frame();
        }
    };

    struct Context {
       private:
        Stream stream;
        ContextInfo info;

       public:
        template <class T>
        std::optional<StreamError> enter_stream(utils::Sequencer<T>& seq, lexer::FileIndex file, auto&& fn) {
            stream.info = &info;
            return stream.enter_stream([&] {
                const auto scope = stream.set_seq(seq, file);
                fn(stream);
            });
        }
    };

    std::shared_ptr<Program> parse(Stream& ctx);
}  // namespace ast
