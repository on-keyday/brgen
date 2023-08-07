/*license*/
#pragma once
#include "ast.h"

namespace ast {
    struct TmpVar : Expr {
        static constexpr ObjectType object_type = ObjectType::tmp_var;
        size_t tmp_index = 0;
        std::shared_ptr<Call> call;

        TmpVar(std::shared_ptr<Call>&& c, size_t tmp)
            : Expr(c->loc, ObjectType::tmp_var), tmp_index(tmp), call(std::move(c)) {
            expr_type = call->expr_type;
        }

        void debug(Debug& buf) const override {
            buf.object([&](auto&& field) {
                field("tmp_var", [&](Debug& d) { d.number(tmp_index); });
            });
        }
    };
}  // namespace ast
