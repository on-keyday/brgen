#include "stream.h"
#include "core/common/error.h"

namespace brgen::ast {
    void Stream::maybe_parse() {
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

    lexer::Loc Stream::last_loc() {
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

    void Stream::shrink() {
        tokens.erase(tokens.begin(), cur);
        cur = tokens.begin();
    }

    std::list<lexer::Token> Stream::take() {
        auto copy = std::move(tokens);
        tokens.clear();
        cur = tokens.begin();
        return copy;
    }

    bool Stream::eos() {
        maybe_parse();
        return cur == tokens.end();
    }

    void Stream::consume() {
        if (eos()) {
            return;
        }
        cur++;
        last_skip.reset();
    }

    lexer::Loc Stream::loc() {
        maybe_parse();
        if (eos()) {
            return last_loc();
        }
        return cur->loc;
    }

    bool Stream::expect_token(lexer::Tag tag) {
        if (eos()) {
            return false;
        }
        return cur->tag == tag;
    }

    bool Stream::expect_token(std::string_view s) {
        if (eos()) {
            return false;
        }
        return cur->token == s;
    }

    std::optional<lexer::Token> Stream::peek_token(std::string_view s) {
        if (!expect_token(s)) {
            return std::nullopt;
        }
        // only copy for fallback
        return *cur;
    }

    std::optional<lexer::Token> Stream::peek_token(lexer::Tag t) {
        if (!expect_token(t)) {
            return std::nullopt;
        }
        // only copy for fallback
        return *cur;
    }

    lexer::Token Stream::peek_token() {
        return *cur;
    }

    std::optional<lexer::Token> Stream::consume_token(std::string_view s) {
        if (auto token = peek_token(s)) {
            consume();
            return token;
        }
        return std::nullopt;
    }

    std::optional<lexer::Token> Stream::consume_token(lexer::Tag t) {
        if (auto token = peek_token(t)) {
            consume();
            return token;
        }
        return std::nullopt;
    }

    LocationError Stream::token_expect_error(std::string_view expected, const char* kind, std::string_view hint) {
        std::string buf;
        appends(buf, "expect token ", kind, " `", expected, "` but found token ");
        if (eos()) {
            append(buf, "`<EOF>`");
        }
        else {
            appends(buf, "`", cur->token, "`(kind: ", lexer::enum_array<lexer::Tag>[int(cur->tag)].second, ")");
            if (cur->token == "]") {
                appends(buf, ", did you forget opening bracket `[`?");
            }
            else if (cur->token == ">") {
                appends(buf, ", did you forget opening angle bracket `<`?");
            }
            else if (cur->token == ")") {
                appends(buf, ", did you forget opening parenthesis `(`?");
            }
            else if (cur->token == "{" || cur->token == "}") {
                appends(buf, ", this language uses python-like blocks, so `{}` is not used.");
            }
        }
        if (hint.size()) {
            appends(buf, " hint: ", hint);
        }

        auto err = error(last_loc(), std::move(buf));
        if (last_skip) {
            return err.error((*last_skip)->loc, "when parsing started here");
        }
        return err;
    }

    LocationError Stream::token_error(lexer::Tag tag, std::string_view hint) {
        return token_expect_error(lexer::to_string(tag), "of kind", hint);
    }

    LocationError Stream::token_error(std::string_view s, std::string_view hint) {
        return token_expect_error(s, "literally", hint);
    }

    lexer::Token Stream::must_consume_token(std::string_view view, std::string_view hint) {
        auto f = consume_token(view);
        if (!f) {
            token_error(view, hint).report();
        }
        return *f;
    }

    lexer::Token Stream::must_consume_token(lexer::Tag tag, std::string_view hint) {
        auto f = consume_token(tag);
        if (!f) {
            token_error(tag, hint).report();
        }
        return *f;
    }

    void Stream::skip_tag(auto... t) {
        maybe_parse();
        auto last = cur;
        while (!eos()) {
            if ((... || expect_token(t))) {
                consume();
                continue;
            }
            break;
        }
        last_skip = (last != cur) ? std::optional{last} : std::nullopt;
    }

    // Tag::space, Tag::comment
    void Stream::skip_space() {
        skip_tag(lexer::Tag::space, lexer::Tag::comment);
    }

    // Tag::space, Tag::line, Tag::comment
    void Stream::skip_line() {
        skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::comment);
    }

    // Tag::space, Tag::line, Tag::indent, Tag::comment
    void Stream::skip_white() {
        skip_tag(lexer::Tag::space, lexer::Tag::line, lexer::Tag::indent, lexer::Tag::comment);
    }

    std::shared_ptr<Node> Stream::get_comments() {
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

    void Stream::backward() {
        if (cur == tokens.begin()) {
            return;
        }
        cur--;
    }

    std::optional<lexer::Token> Stream::prev_token() {
        if (cur == tokens.begin()) {
            return std::nullopt;
        }
        return *std::prev(cur);
    }

    void Stream::set_collect_comments(bool b) {
        collect_comments = b;
    }

    void Stream::set_regex_mode(bool b) {
        lex_option.regex_mode = b;
    }

}  // namespace brgen::ast