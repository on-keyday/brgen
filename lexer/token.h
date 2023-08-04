/*license*/
#pragma once
#include <comb2/pos.h>
#include <string>

namespace lexer {
    using Pos = utils::comb2::Pos;

    struct Loc {
        Pos pos;
        std::uint64_t file = 0;  // file index
    };

    enum class Tag {
        keyword,
        puncut,
        ident,
        str_literal,
        int_literal,
        indent,
        comment,
        space,
        line,
        unknwon,
    };

    struct Token {
        Tag tag = Tag::unknwon;
        std::string token;
        Loc loc;
    };

}  // namespace lexer