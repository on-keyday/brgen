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

namespace lexer {

    namespace internal {
        namespace cps = utils::comb2::composite;
        using namespace utils::comb2::ops;
        constexpr auto space = cps::tab | cps::space;
        constexpr auto spaces = str(Tag::space, *(cps::tab | cps::space));
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
                                                      space | line |
                                                      cps::decimal_number) &
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
            return [l](auto&& seq, auto&& ctx) {
                return lex(seq, ctx, l);
            };
        }

        constexpr auto parse_one = one_token_lexer();

        constexpr auto test_text = R"a(
fmt QUICPacket: 
   form :b1
   if form:
      :LongPacket
   else:
      :OneRTTPacket
   

fmt LongPacket:
   fixed :b1
   long_packet_type :b2
   reserved :b2
   packet_number_length :b2
   version :u32
   

fmt ConnectionID:
   id :[]byte
   fn encode():
        pass      


fmt Varint:
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

        constexpr auto check_lexer() {
            auto seq = utils::make_ref_seq(test_text);
            auto ctx = utils::comb2::test::TestContext<Tag>{};

            while (parse_one(seq, ctx) == utils::comb2::Status::match) {
                auto t = ctx.str_tag;
            }
            return seq.eos();
        }

        static_assert(check_lexer());

    }  // namespace internal

}  // namespace lexer
