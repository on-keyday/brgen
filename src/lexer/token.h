/*license*/
#pragma once
#include <comb2/pos.h>
#include <string>

namespace lexer {
    using Pos = utils::comb2::Pos;

    using FileIndex = std::uint64_t;

    constexpr FileIndex builtin = 0;

    struct Loc {
        Pos pos;
        FileIndex file = 0;  // file index
    };

    enum class Tag {
        indent,
        space,
        line,
        puncut,
        int_literal,
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
        "puncut",
        "int_literal",
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

}  // namespace lexer