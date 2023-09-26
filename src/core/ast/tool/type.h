/*license*/
#pragma once

#include "../ast.h"
#include "eval.h"

namespace brgen::ast::tool {

    struct ArrayDesc {
        std::shared_ptr<Type> base_type;
        std::shared_ptr<Expr> length;
        EResult length_eval;
    };

    inline std::optional<ArrayDesc> is_array_type(auto&& typ, Evaluator& eval) {
        if (auto arr = ast::as<ArrayType>(typ)) {
            ArrayDesc desc;
            desc.base_type = arr->base_type;
            desc.length = arr->length;
            if (arr->length) {
                desc.length_eval = eval.eval_as<EResultType::integer>(arr->length);
            }
        }
        return std::nullopt;
    }
}  // namespace brgen::ast::tool
