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
#include "nodes.h"
#include "../common/file.h"
#include "core/common/error.h"
#include "core/lexer/lexer_enum.h"

namespace brgen::nast {

    struct Stream {
       private:
        std::list<lexer::LiteToken> tokens;
        // バッファがバイト列ならここが非空になり、本文はここから切り出せる
        // (utf8 入力 / utf8 解釈)。空なら要素型が byte でないので File 経由。
        futils::view::rvec source_;
        using iterator = typename std::list<lexer::LiteToken>::iterator;
        iterator cur;
        std::optional<iterator> last_skip;
        File* input;
        size_t line = 1;
        size_t col = 1;
        std::vector<CommentRange> comments;
        bool collect_comments = false;
        lexer::Option lex_option;
        std::optional<iterator> prev_skip_pos;

        Stream() = default;
        friend struct Context;

        void maybe_parse();

        lexer::Loc last_loc();

       public:
        // discard tokens before cur
        void shrink();

        std::list<lexer::LiteToken> take();

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

        std::optional<lexer::LiteToken> peek_token(std::string_view s);

        std::optional<lexer::LiteToken> peek_token(lexer::Tag t);

        lexer::LiteToken peek_token();

        std::optional<lexer::LiteToken> consume_token(std::string_view s);

        std::optional<lexer::LiteToken> consume_token(lexer::Tag t);

       private:
        [[nodiscard]] LocationError token_expect_error(std::string_view expected, const char* kind, std::string_view hint);

       public:
        [[nodiscard]] LocationError token_error(lexer::Tag tag, std::string_view hint);

        [[nodiscard]] LocationError token_error(std::string_view s, std::string_view hint);

        lexer::LiteToken must_consume_token(std ::string_view view, std::string_view hint);

        lexer::LiteToken must_consume_token(lexer::Tag tag, std::string_view hint);

       private:
        void skip_tag(auto... t);

       public:
        // Tag::space, Tag::comment
        void skip_space();

        // Tag::space, Tag::comment
        void skip_space_comment();

        // Tag::space, Tag::line, Tag::comment
        void skip_line();

        // Tag::space, Tag::line, Tag::indent, Tag::comment
        void skip_white();

        // back to last skip position if exists
        void recover_to_prev_skip();

        // Node<Comment> get_comments();

        void backward();
        std::optional<lexer::LiteToken> prev_token();

        // トークンの本文が ASCII 文字列と等しいか。確保をしない。
        // バイト列ならその場で、そうでなければ要素型を知っている File で比べる。
        bool token_is(const lexer::LiteToken& t, std::string_view ascii) const {
            if (!source_.null()) {
                return t.loc.pos.len() == ascii.size() &&
                       std::string_view(reinterpret_cast<const char*>(source_.data()) + t.loc.pos.begin,
                                        t.loc.pos.len()) == ascii;
            }
            return input->token_equal(t.loc.pos, ascii);
        }

        // トークンの本文。バイト列でなければ File が変換する。
        // 本文を実際に要るのは、ノードに入れるトークンと診断だけ。
        std::string text(const lexer::LiteToken& t) const {
            if (!source_.null()) {
                return std::string(reinterpret_cast<const char*>(source_.data()) + t.loc.pos.begin,
                                   t.loc.pos.len());
            }
            return input->token_text(t.loc.pos);
        }

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
            // 本文はここから切り出す。試作なので utf8 入力 / utf8 解釈だけ。
            s.source_ = file->source();
            return s.enter_stream(parser);
        }
    };

}  // namespace brgen::nast
