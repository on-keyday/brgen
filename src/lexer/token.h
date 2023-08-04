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
        indent,
        space,
        line,
        puncut,
        int_literal,
        str_literal,
        keyword,
        ident,
        comment,
        unknwon,
    };

    struct Token {
        Tag tag = Tag::unknwon;
        std::string token;
        Loc loc;
    };

}  // namespace lexer