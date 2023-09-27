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

    inline std::optional<ArrayDesc> is_array_type(auto&& typ, Evaluator& eval, bool omit_eval = false) {
        if (auto arr = ast::as<ArrayType>(typ)) {
            ArrayDesc desc;
            desc.base_type = arr->base_type;
            desc.length = arr->length;
            if (arr->length && !omit_eval) {
                desc.length_eval = eval.eval_as<EResultType::integer>(arr->length);
            }
            return desc;
        }
        return std::nullopt;
    }

    struct IntDesc {
        size_t bit_size = 0;
        bool is_signed = false;
        Endian endian = Endian::unspec;
    };

    inline std::optional<IntDesc> is_int_type(auto&& typ) {
        if (auto i = ast::as<IntType>(typ)) {
            IntDesc desc;
            desc.bit_size = i->bit_size;
            desc.is_signed = i->is_signed;
            desc.endian = i->endian;
            return desc;
        }
        return std::nullopt;
    }
}  // namespace brgen::ast::tool
