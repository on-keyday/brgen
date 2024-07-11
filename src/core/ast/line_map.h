/*license*/
#pragma once
#include <core/lexer/token.h>

namespace brgen::ast {
    struct LineMap {
        brgen::lexer::Loc loc;
        size_t line;
    };

    void as_json(LineMap& mp, auto&& obj) {
        auto field = obj.object();
        field("loc", mp.loc);
        field("line", mp.line);
    }

}  // namespace brgen::ast