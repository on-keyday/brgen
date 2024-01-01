/*license*/
#pragma once
#include <comb2/pos.h>
#include <string>

namespace brgen::lexer {
    using Pos = futils::comb2::Pos;

    using FileIndex = std::uint64_t;

    constexpr FileIndex builtin = 0;

    struct Loc {
        Pos pos;
        FileIndex file = 0;  // file index
        size_t line = 0;
        size_t col = 0;
    };

    constexpr bool operator==(const Loc& lhs, const Loc& rhs) {
        return lhs.pos == rhs.pos && lhs.file == rhs.file && lhs.line == rhs.line && lhs.col == rhs.col;
    }

    constexpr void as_json(Loc l, auto&& buf) {
        auto field = buf.object();
        field("pos", [&] {
            auto field = buf.object();
            field("begin", l.pos.begin);
            field("end", l.pos.end);
        });
        field("file", l.file);
        field("line", l.line);
        field("col", l.col);
    }

    enum class Tag {
        indent,
        space,
        line,
        punct,
        int_literal,
        bool_literal,
        str_literal,
        keyword,
        ident,
        comment,
        error,
        unknwon,
    };

    constexpr const char* tag_str[] = {
        "indent",
        "space",
        "line",
        "punct",
        "int_literal",
        "bool_literal",
        "str_literal",
        "keyword",
        "ident",
        "comment",
        "error",
        "unknown",
    };

    constexpr auto tag_count = sizeof(tag_str) / sizeof(tag_str[0]);

    struct Token {
        Tag tag = Tag::unknwon;
        std::string token;
        Loc loc;
    };

    constexpr void as_json(Tag tag, auto&& buf) {
        buf.string(tag_str[int(tag)]);
    }

    constexpr void as_json(const Token& token, auto&& buf) {
        auto field = buf.object();
        field("tag", token.tag);
        field("token", token.token);
        field("loc", token.loc);
    }

    constexpr auto to_string(Tag t) {
        return tag_str[int(t)];
    }

}  // namespace brgen::lexer
