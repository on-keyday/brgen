/*license*/
#pragma once
#include <comb2/composite/range.h>
#include <comb2/basic/proxy.h>
#include <comb2/basic/group.h>
#include <comb2/basic/peek.h>
#include <comb2/composite/comment.h>
#include <comb2/composite/number.h>
#include <comb2/composite/string.h>
#include "token.h"
#include <optional>
#include "lexer_test_text.h"

namespace lexer {

    namespace internal {
        namespace cps = utils::comb2::composite;
        using namespace utils::comb2::ops;
        constexpr auto space = cps::tab | cps::space;
        constexpr auto spaces = str(Tag::space, ~(cps::tab | cps::space));
        constexpr auto line = str(Tag::line, cps::eol);
        constexpr auto indent = str(Tag::indent, bol & ~(cps::tab | cps::space) & not_(cps::eol));
        constexpr auto comment = str(Tag::comment, cps::shell_comment);

        constexpr auto int_literal = str(Tag::int_literal, cps::hex_integer | cps::oct_integer | cps::bin_integer | cps::dec_integer);
        constexpr auto str_literal = str(Tag::str_literal, cps::c_str | cps::char_str);

        constexpr auto puncut(auto&&... args) {
            return str(Tag::puncut, (... | lit(args)));
        }

        constexpr auto keyword(auto&&... args) {
            auto p = method_proxy(puncuts);
            return str(Tag::keyword, (... | lit(args)) & peek(space | line | p));
        }

        constexpr auto ident = str(Tag::ident, ~(not_(method_proxy(puncuts) |
                                                      space | line) &
                                                 uany));

        constexpr auto keywords = keyword("fmt", "if", "else", "match", "fn");
        constexpr auto puncuts = puncut(
            "#", "\"", "\'",  // added but maybe not used
            ":", "(", ")", "[", "]",
            "=>", "=",
            "..", ".",
            ">>", "<<", "~",
            "&", "|");

        constexpr auto one_token_lexer() {
            auto p = method_proxy(puncuts);
            auto lex = indent | spaces | line | comment | int_literal | str_literal | p | keywords | ident;
            struct L {
                decltype(puncuts) puncuts;
            } l{puncuts};
            return [l, lex](auto&& seq, auto&& ctx) {
                return lex(seq, ctx, l);
            };
        }

        constexpr auto parse_one = one_token_lexer();

        constexpr auto check_lexer() {
            auto seq = utils::make_ref_seq(test_text);
            auto ctx = utils::comb2::test::TestContext<Tag>{};
            Tag m[] = {
                Tag::line,
                Tag::keyword,
                Tag::space,
                Tag::ident,
                Tag::puncut,
                Tag::space,
                Tag::line,
                Tag::indent,
                Tag::ident,
                Tag::space,
                Tag::puncut,
                Tag::ident,
                Tag::line,
                Tag::indent,
                Tag::keyword,
                Tag::space,
                Tag::ident,
                Tag::puncut,
                Tag::line,
                Tag::indent,
                Tag::puncut,
                Tag::ident,
                Tag::line,
                Tag::indent,
                Tag::keyword,
                Tag::puncut,
                Tag::line,
                Tag::indent,
                Tag::puncut,
                Tag::ident,
                Tag::line,
                Tag::space,
                Tag::line,
            };
            size_t i = 0;
            auto len = sizeof(m) / sizeof(m[0]);
            while (parse_one(seq, ctx) == utils::comb2::Status::match) {
                if (i < len) {
                    if (m[i] != ctx.str_tag) {
                        utils::comb2::test::error_if_constexpr(i, m[i], ctx.str_tag);
                    }
                    i++;
                }
            }
            return seq.eos();
        }

        static_assert(check_lexer());

    }  // namespace internal

    template <class T>
    std::optional<Token> parse_one(utils::Sequencer<T>& seq, std::uint64_t file) {
        auto ctx = utils::comb2::LexContext<Tag, std::string>{};
        if (auto res = internal::parse_one(seq, ctx); res != utils::comb2::Status::match) {
            if (res == utils::comb2::Status::fatal) {
                Token tok;
                tok.tag = Tag::error;
                tok.loc.file = file;
                tok.loc.pos = {seq.rptr, seq.rptr + 1};
                tok.token = std::move(ctx.errbuf);
                return tok;
            }
            if (!seq.eos()) {
                Token tok;
                tok.tag = Tag::error;
                tok.loc.file = file;
                tok.loc.pos = {seq.rptr, seq.rptr + 1};
                tok.token = "expect eof but not";
                return tok;
            }
            return std::nullopt;
        }
        Token tok;
        tok.tag = ctx.str_tag;
        tok.loc.file = file;
        tok.loc.pos = ctx.str_pos;
        seq.rptr = ctx.str_pos.begin;
        tok.token.reserve(ctx.str_pos.len());
        for (; seq.rptr < ctx.str_pos.end; seq.rptr++) {
            tok.token.push_back(seq.current());
        }
        return tok;
    }

}  // namespace lexer
