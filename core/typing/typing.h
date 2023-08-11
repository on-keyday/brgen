/*license*/
#pragma once
#include "../lexer/token.h"

namespace brgen::typing {

    struct DefinedError {
        std::string ident;
        lexer::Loc duplicated;
        lexer::Loc defined_at;
    };

    struct NotDefinedError {
        std::string ident;
        lexer::Loc ref_at;
    };

    struct AssignError {
        std::string ident;
        lexer::Loc duplicated;
        lexer::Loc defined_at;
    };

    struct NotBoolError {
        lexer::Loc expr_loc;
    };
}  // namespace brgen::typing