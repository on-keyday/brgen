/*license*/
#pragma once
#include <comb2/composite/range.h>
#include <comb2/basic/proxy.h>
#include "lexer.h"

namespace lexer {
    namespace cps = utils::comb2::composite;
    constexpr auto space = cps::space;

    constexpr auto puncut(auto&&... args) {
        return str(Tag::puncut, (... | lit(args)));
    }

    constexpr auto keyword(auto&&... args) {
        auto p = method_proxy(puncuts);
        auto s = method_proxy();
        return str(Tag::keyword, (... | lit(args)) & not_(cps::c_ident_next));
    }

    constexpr auto keywords = keyword("fmt", "if", "else", "match", "fn");
    constexpr auto puncuts = puncut(
        ":", "(", ")", "[", "]",
        "=>", "=",
        "..", ".",
        ">>", "<<", "~",
        "&", "|");

}  // namespace lexer
