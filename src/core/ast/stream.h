/*license*/
#pragma once
#include "../lexer/token.h"
#include "../lexer/lexer.h"
#include <list>
#include <optional>
#include <helper/defer.h>
#include <code/src_location.h>
#include <map>
#include <string_view>
#include "ast.h"
#include "../common/file.h"
#include "core/common/error.h"
#include "core/lexer/lexer_enum.h"

namespace brgen::ast {

    struct Stream {
       private:
        std::list<lexer::Token> tokens;
        using iterator = typename std::list<lexer::Token>::iterator;
        iterator cur;
        std::optional<iterator> last_skip;
        File* input;
        size_t line = 1;
        size_t col = 1;
        std::vector<std::shared_ptr<Comment>> comments;
        bool collect_comments = false;
        lexer::Option lex_option;

        Stream() = default;
        friend struct Context;

        void maybe_parse();

        lexer::Loc last_loc();

       public:
        // discard tokens before cur
        void shrink();

        std::list<lexer::Token> take();

        [[noreturn]] void report_error(auto&&... data) {
            error(last_loc(), "parser error: ", data...).report();
        }

        [[noreturn]] void report_error(lexer::Loc loc, auto&&... data) {
            error(loc, "parser error: ", data...).report();
        }

        // end of stream
        bool eos();

        void consume();

        lexer::Loc loc();

        bool expect_token(lexer::Tag tag);
        bool expect_token(std::string_view s);

        std::optional<lexer::Token> peek_token(std::string_view s);

        std::optional<lexer::Token> peek_token(lexer::Tag t);

        lexer::Token peek_token();

        std::optional<lexer::Token> consume_token(std::string_view s);

        std::optional<lexer::Token> consume_token(lexer::Tag t);

       private:
        [[nodiscard]] LocationError token_expect_error(std::string_view expected, const char* kind, std::string_view hint);

       public:
        [[nodiscard]] LocationError token_error(lexer::Tag tag, std::string_view hint);

        [[nodiscard]] LocationError token_error(std::string_view s, std::string_view hint);

        lexer::Token must_consume_token(std ::string_view view, std::string_view hint);

        lexer::Token must_consume_token(lexer::Tag tag, std::string_view hint);

       private:
        void skip_tag(auto... t);

       public:
        // Tag::space, Tag::comment
        void skip_space();

        // Tag::space, Tag::line, Tag::comment
        void skip_line();

        // Tag::space, Tag::line, Tag::indent, Tag::comment
        void skip_white();

        std::shared_ptr<Node> get_comments();

        void backward();
        std::optional<lexer::Token> prev_token();

        void set_collect_comments(bool b);

        void set_regex_mode(bool b);

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
