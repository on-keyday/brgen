/*license*/
#pragma once
#include "../lexer/token.h"

namespace brgen::typing {

    struct DefinedError {
        std::string ident;
        lexer::Loc duplicated_at;
        lexer::Loc defined_at;
    };

    struct NotDefinedError {
        std::string ident;
        lexer::Loc ref_at;
    };

    struct AssignError {
        std::string ident;
        lexer::Loc duplicated_at;
        lexer::Loc defined_at;
    };

    struct NotBoolError {
        lexer::Loc expr_loc;
    };

    struct BinaryOpTypeError {
        ast::BinaryOp op;
        lexer::Loc loc;
    };

    struct NotEqualTypeError {
        lexer::Loc a;
        lexer::Loc b;
    };

    struct UnsupportedError {
        lexer::Loc l;
    };

    struct TooLargeError {
        lexer::Loc l;
    };

    void typing_object(const std::shared_ptr<ast::Node>& ty);
}  // namespace brgen::typing
