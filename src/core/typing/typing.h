/*license*/
#pragma once
#include "../lexer/token.h"
#include "../common/file.h"
#include "../ast/ast.h"

namespace brgen::typing {

    void typing_object(const std::shared_ptr<ast::Node>& ty);

    inline either::expected<void, std::string> typing_with_error(const std::shared_ptr<ast::Node>& ty, FileSet& input) {
        try {
            typing_object(ty);
        } catch (LocationError& e) {
            std::string err;
            for (auto& loc : e.locations) {
                err += input.error(std::move(loc.msg), loc.loc).to_string() + "\n";
            }
            return unexpect(err);
        }
        return {};
    }
}  // namespace brgen::typing
