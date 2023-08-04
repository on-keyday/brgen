/*license*/
#pragma once
#include <string>
#include <comb2/pos.h>

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
        literal,
        indent,
        comment,
        space,
        unknwon,
    };

    struct Token {
        Tag tag = Tag::unknwon;
        std::string token;
        Loc loc;
    };
}  // namespace lexer
