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
#include <array>
#include <string_view>
#include <utility>
#include <comb2/internal/test.h>

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
        constexpr auto int_literal = str(Tag::int_literal, (cps::hex_integer_weak | cps::oct_integer_weak | cps::bin_integer_weak | cps::dec_integer) & filter_keyword);
        constexpr auto partial_int_literal = str(Tag::partial_int_literal, (cps::hex_integer_weak | cps::oct_integer_weak | cps::bin_integer_weak | cps::dec_integer) & method_proxy(ident));
        constexpr auto str_literal = str(Tag::str_literal, cps::c_str_weak);
        constexpr auto partial_str_literal = str(Tag::partial_str_literal, cps::c_str_partial);
        constexpr auto char_literal = str(Tag::char_literal, cps::char_str_weak);
        constexpr auto partial_char_literal = str(Tag::partial_char_literal, cps::char_str_partial);
        constexpr auto regex_literal = str(Tag::regex_literal, cps::js_regex_str_weak & -(~oneof("dgimsuy") & filter_keyword));
        constexpr auto partial_regex_literal = str(Tag::partial_regex_literal, cps::js_regex_str_partial);
        constexpr auto bool_literal = str(Tag::bool_literal, (lit("true") | lit("false")) & filter_keyword);

        constexpr auto puncts(auto&&... args) {
            return str(Tag::punct, (... | lit(args)));
        }

        constexpr auto keyword(auto&&... args) {
            return str(Tag::keyword, (... | lit(args)) & filter_keyword);
        }

        // punct の一覧はここだけ。punct_ と punct_head_chars の両方をここから作る。
        // 2 か所に書くと、記号を足したときに識別子側の定義が古いまま残る。
        constexpr const char* punct_strings[] = {
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
            "<=", ">=", "<", ">", "?", ","};

        template <std::size_t... I>
        constexpr auto make_puncts(std::index_sequence<I...>) {
            return puncts(punct_strings[I]...);
        }

        constexpr auto punct_ = make_puncts(std::make_index_sequence<std::size(punct_strings)>{});

        // punct の 1 文字目を重複なく集めたもの。
        // この名前空間は futils::comb2::ops を using している。そこには制約の無い
        // 単項 operator* (logic.h, 実体は -~a) があり、range-for が展開する
        // *__begin がそちらに取られる。コンテナは添字で回すこと。
        constexpr std::size_t punct_count = std::size(punct_strings);

        constexpr std::size_t count_punct_heads() {
            std::size_t n = 0;
            for (std::size_t i = 0; i < punct_count; i++) {
                bool dup = false;
                for (std::size_t j = 0; j < i; j++) {
                    if (punct_strings[j][0] == punct_strings[i][0]) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    n++;
                }
            }
            return n;
        }

        // NUL 終端にして const char* として渡す。comb2 の oneof は
        // ポインタなら strlen して 1 文字ずつ見る (既存の oneof("dgimsuy") と同じ形)。
        constexpr auto punct_head_chars = [] {
            std::array<char, count_punct_heads() + 1> out{};
            std::size_t n = 0;
            for (std::size_t i = 0; i < punct_count; i++) {
                bool dup = false;
                for (std::size_t j = 0; j < n; j++) {
                    if (out[j] == punct_strings[i][0]) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    out[n++] = punct_strings[i][0];
                }
            }
            return out;
        }();

        // 1 文字だけの punct が全ての 1 文字目について在ること。
        // これが成り立つ間だけ「1 文字目が punct_head_chars」と「punct が一致」が
        // 同値になり、下の ident_ が space_or_punct 版と同じ結果を出す。
        constexpr bool every_punct_head_is_a_punct() {
            for (std::size_t i = 0; i + 1 < punct_head_chars.size(); i++) {
                bool found = false;
                for (std::size_t j = 0; j < punct_count; j++) {
                    if (punct_strings[j][0] == punct_head_chars[i] &&
                        punct_strings[j][1] == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return false;
                }
            }
            return true;
        }

        static_assert(every_punct_head_is_a_punct(),
                      "every punctuation's first character must itself be a punctuation; "
                      "otherwise ident_ stops where punct does not actually match");

        // 識別子は「空白でも改行でも punct の 1 文字目でもない文字」の並び。
        //
        // 元は not_(space_or_punct) と書いていて、space_or_punct には punct の
        // 全リテラルが入っていた。つまり識別子 1 文字ごとに 50 個超のリテラルを
        // 順に照合していた。example/ の実測で識別子は 1 文字あたり 2.7 us かかり、
        // punct トークン 1 個分 (2.3 us/char) とほぼ同じだった。
        // 1 文字目だけを見る形にすると字句解析が 294 ms -> 154 ms (1.91 倍) になり、
        // トークン列は 143251 個すべて一致した。
        constexpr auto punct_head = oneof(punct_head_chars.data());

        // punct が一致する位置と punct_head が一致する位置が一致すること。
        // ident_ はこの同値性の上に立っているので、compile time で確かめる。
        // 上の static_assert は「1 文字の punct が在る」という作り方の条件で、
        // こちらは結果として同値になっていることの検査。
        constexpr bool punct_head_matches_punct() {
            for (int c = 1; c < 128; c++) {
                const char buf[2] = {char(c), 0};
                // make_ref_seq は参照で持つので、view を名前付きで置く。
                // 一時オブジェクトを渡すと定数評価の途中で寿命が切れる。
                const std::string_view one(buf, 1);
                auto s1 = futils::make_ref_seq(one);
                auto s2 = futils::make_ref_seq(one);
                auto ctx1 = futils::comb2::test::TestContext<Tag>{};
                auto ctx2 = futils::comb2::test::TestContext<Tag>{};
                bool as_punct = punct_(s1, ctx1, 0) == futils::comb2::Status::match;
                bool as_head = punct_head(s2, ctx2, 0) == futils::comb2::Status::match;
                if (as_punct != as_head) {
                    return false;
                }
            }
            return true;
        }

        static_assert(punct_head_matches_punct(),
                      "punct_head must match exactly where punct does; ident_ relies on it");

        constexpr auto ident_ = str(Tag::ident, ~(not_(space | line | punct_head | eos) &
                                                  uany));

        constexpr auto keywords = keyword(
            "format", "if", "elif", "else", "match", "fn", "for", "enum",
            "input", "output", "config", "true", "false",
            "return", "break", "continue", "state", "self");

        struct Option {
            decltype(punct_) punct{punct_};
            decltype(ident_) ident{ident_};
            bool regex_mode = false;
        };

        constexpr auto one_token_lexer() {
            auto p = method_proxy(punct);
            auto ident = method_proxy(ident);
            auto regex = conditional_method(regex_mode, futils::comb2::Status::not_match, regex_literal);
            auto partial_regex = conditional_method(regex_mode, futils::comb2::Status::not_match, partial_regex_literal);
            auto lex = indent |
                       spaces |
                       line |
                       comment |
                       int_literal |
                       partial_int_literal |
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

    // 本文を切り出さない版。tag と loc だけ返す。
    // 本文が要る側は loc.pos とバッファから取る。
    //
    // ただし Tag::error のトークンだけは別。そこでの本文は入力の一部ではなく
    // エラーメッセージ本体なので、loc.pos からは取れない。err に入れて返す。
    //
    // parse_one が本文を入力以外から入れるのは 3 か所:
    //   ctx.errbuf              combinator が fatal を返した
    //   "expect eof but not"    どの選択肢にも当たらず、末尾でもない
    //   "invalid utf sequence"  TokenBuf への変換に失敗した
    // 前 2 つはここでも同じように返す。3 つ目は変換をしないので起きない。
    // 変換が要る経路 (utf16/utf32 解釈) をこの関数で扱うなら、そこも要る。
    template <class T>
    std::optional<LiteToken> parse_one_no_text(futils::Sequencer<T>& seq, std::uint64_t file, Option opt,
                                               std::string* err = nullptr) {
        internal::Option option;
        option.regex_mode = opt.regex_mode;
        auto ctx = futils::comb2::LexContext<Tag, std::string>{};
        if (auto res = internal::parse_one(seq, ctx, option); res != futils::comb2::Status::match) {
            if (res == futils::comb2::Status::fatal) {
                LiteToken tok;
                tok.tag = Tag::error;
                tok.loc.file = file;
                tok.loc.pos = {seq.rptr, seq.rptr + 1};
                if (err) {
                    *err = std::move(ctx.errbuf);
                }
                return tok;
            }
            if (!seq.eos()) {
                LiteToken tok;
                tok.tag = Tag::error;
                tok.loc.file = file;
                tok.loc.pos = {seq.rptr, seq.rptr + 1};
                if (err) {
                    *err = "expect eof but not";
                }
                return tok;
            }
            return std::nullopt;
        }
        LiteToken tok;
        tok.tag = ctx.str_tag;
        tok.loc.file = file;
        tok.loc.pos = ctx.str_pos;
        seq.rptr = ctx.str_pos.end;
        return tok;
    }

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
