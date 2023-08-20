/*license*/
#pragma once
#include "../lexer/token.h"
#include "../common/file.h"
#include "../ast/ast.h"

namespace brgen::typing {

    void typing_object(const std::shared_ptr<ast::Node>& ty);

    inline result<void> typing_with_error(const std::shared_ptr<ast::Node>& ty) {
        try {
            typing_object(ty);
        } catch (LocationError& e) {
            return unexpect(e);
        }
        return {};
    }

}  // namespace brgen::typing
