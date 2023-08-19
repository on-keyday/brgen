/*license*/
#pragma once
#include <comb2/pos.h>
#include <string>

namespace brgen::lexer {
    using Pos = utils::comb2::Pos;

    using FileIndex = std::uint64_t;

    constexpr FileIndex builtin = 0;

    struct Loc {
        Pos pos;
        FileIndex file = 0;  // file index
    };

    constexpr void as_json(Loc l, auto&& buf) {
        auto field = buf.object();
        field("pos", [&] {
            auto field = buf.object();
            field("begin", l.pos.begin);
            field("end", l.pos.end);
        });
        field("file", l.file);
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

    struct Token {
        Tag tag = Tag::unknwon;
        std::string token;
        Loc loc;
    };

}  // namespace brgen::lexer