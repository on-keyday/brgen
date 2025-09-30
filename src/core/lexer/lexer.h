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

namespace brgen::lexer {

    namespace internal {
        namespace cps = futils::comb2::composite;
        using namespace futils::comb2::ops;
        constexpr auto space = cps::tab | cps::space;
        constexpr auto spaces = str(Tag::space, ~(cps::tab | cps::space));
        constexpr auto line = str(Tag::line, cps::eol);
        constexpr auto indent = str(Tag::indent, bol & ~(cps::tab | cps::space) & not_(lit('#') /*ignore comment*/ | cps::eol | eos));
        constexpr auto comment = str(Tag::comment, cps::shell_comment);

        constexpr auto space_or_punct = space | line | method_proxy(punct) | eos;
        constexpr auto filter_keyword = peek(space_or_punct);
        constexpr auto int_literal = str(Tag::int_literal, (cps::hex_integer | cps::oct_integer | cps::bin_integer | cps::dec_integer) & +filter_keyword);
        constexpr auto str_literal = str(Tag::str_literal, cps::c_str_weak);
        constexpr auto partial_str_literal = str(Tag::partial_str_literal, cps::c_str_partial);
        constexpr auto char_literal = str(Tag::char_literal, cps::char_str_weak);
        constexpr auto partial_char_literal = str(Tag::partial_char_literal, cps::char_str_partial);
        constexpr auto regex_literal = str(Tag::regex_literal, cps::js_regex_str_weak & -(~oneof("dgimsuy") & +filter_keyword));
        constexpr auto partial_regex_literal = str(Tag::partial_regex_literal, cps::js_regex_str_partial);
        constexpr auto bool_literal = str(Tag::bool_literal, (lit("true") | lit("false")) & filter_keyword);

        constexpr auto puncts(auto&&... args) {
            return str(Tag::punct, (... | lit(args)));
        }

        constexpr auto keyword(auto&&... args) {
            return str(Tag::keyword, (... | lit(args)) & filter_keyword);
        }

        constexpr auto ident = str(Tag::ident, ~(not_(space_or_punct) &
                                                 uany));

        constexpr auto keywords = keyword(
            "format", "if", "elif", "else", "match", "fn", "for", "enum",
            "input", "output", "config", "true", "false",
            "return", "break", "continue", "state");
        constexpr auto punct_ = puncts(
            "#", "\"", "\'", "$" /*for builtin method*/,  // added but maybe not used
            "::=", ":=",
            ":", ";", "(", ")", "[", "]", "{", "}", /*{} is not used currently but reserved*/
            "=>", "==", "=",
            "..=", "..", ".", "->",
            "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", ">>>=", "<<<=", "<<=", ">>=",
            ">>>", "<<<", ">>", "<<", "~",
            "&&", "||", "&", "|",
            "!=", "!",
            "+", "-", "*", "/", "%", "^",
            "<=", ">=", "<", ">", "?", ",");

        struct Option {
            decltype(punct_) punct{punct_};
            bool regex_mode = false;
        };

        constexpr auto one_token_lexer() {
            auto p = method_proxy(punct);
            auto regex = conditional_method(regex_mode, futils::comb2::Status::not_match, regex_literal);
            auto partial_regex = conditional_method(regex_mode, futils::comb2::Status::not_match, partial_regex_literal);
            auto lex = indent |
                       spaces |
                       line |
                       comment |
                       int_literal |
                       str_literal |
                       partial_str_literal |
                       regex |
                       partial_regex |
                       char_literal |
                       partial_char_literal |
                       p |
                       bool_literal |
                       keywords |
                       ident;
            return lex;
        }

        constexpr auto parse_one = one_token_lexer();

        constexpr auto check_lexer() {
            constexpr auto test_text = R"a(
format QUICPacket: 
   form :b1
   if form:
      :LongPacket
   else:
      :OneRTTPacket
   

format LongPacket:
   fixed :b1
   long_packet_type :b2
   reserved :b2
   packet_number_length :b2
   version :u32
   

format ConnectionID:
   id :[]byte
   fn encode():
        pass      


format Varint:
   value :u64
   fn decode(input):
      p = input[0]
      value = match p&0xC0 >> 6:
         0 => input.u8() ~ msb(2)
         1 => input.u16() ~ msb(2)
         2 => input.u32() ~ msb(2)
         3 => input.u64() ~ msb(2)

   fn encode(output):
      match value:
         ..0x40 => output.u8(value.u8())
         ..0x4000 => output.u16(value.u16() | msb(2,1))
         ..0x40000000 => output.u32(value.u32() | msb(2,2))
         ..0x4000000000000000 => output.u64(value | msb(2,3))
         _ => error("too large number")

)a";

            auto seq = futils::make_ref_seq(test_text);
            auto ctx = futils::comb2::test::TestContext<Tag>{};
            Tag m[] = {
                Tag::line,
                Tag::keyword,
                Tag::space,
                Tag::ident,
                Tag::punct,
                Tag::space,
                Tag::line,
                Tag::indent,
                Tag::ident,
                Tag::space,
                Tag::punct,
                Tag::ident,
                Tag::line,
                Tag::indent,
                Tag::keyword,
                Tag::space,
                Tag::ident,
                Tag::punct,
                Tag::line,
                Tag::indent,
                Tag::punct,
                Tag::ident,
                Tag::line,
                Tag::indent,
                Tag::keyword,
                Tag::punct,
                Tag::line,
                Tag::indent,
                Tag::punct,
                Tag::ident,
                Tag::line,
                Tag::space,
                Tag::line,
            };
            size_t i = 0;
            auto len = sizeof(m) / sizeof(m[0]);
            while (parse_one(seq, ctx, Option{}) == futils::comb2::Status::match) {
                if (i < len) {
                    if (m[i] != ctx.str_tag) {
                        futils::comb2::test::error_if_constexpr(i, m[i], ctx.str_tag);
                    }
                    i++;
                }
            }
            return seq.eos();
        }

    }  // namespace internal

    struct Option {
        bool regex_mode = false;
    };

    template <class TokenBuf = std::string, class T>
    std::optional<Token> parse_one(futils::Sequencer<T>& seq, std::uint64_t file, Option opt) {
        internal::Option option;
        option.regex_mode = opt.regex_mode;
        auto ctx = futils::comb2::LexContext<Tag, std::string>{};
        if (auto res = internal::parse_one(seq, ctx, option); res != futils::comb2::Status::match) {
            if (res == futils::comb2::Status::fatal) {
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
        TokenBuf buf;
        buf.resize(ctx.str_pos.len());
        auto ptr = buf.data();
        for (; seq.rptr < ctx.str_pos.end; seq.rptr++) {
            *ptr++ = seq.buf.buffer[seq.rptr];  // HACK(on-keyday): use buffer directly
        }
        if constexpr (std::is_same_v<TokenBuf, std::string>) {
            tok.token = std::move(buf);
        }
        else {
            auto err = futils::utf::convert<0, 1>(buf, tok.token, false, false);
            if (!err) {
                tok.tag = Tag::error;
                tok.token = "invalid utf sequence";
            }
        }
        return tok;
    }

}  // namespace brgen::lexer
