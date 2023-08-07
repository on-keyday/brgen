/*license*/
#pragma once
#include "ast.h"
#include "translated.h"

namespace ast {
    void traverse(auto&& t, auto&& fn) {
        Object* o = std::to_address(t);
#define SWITCH   \
    if (false) { \
    }
#define CASE(T) else if (T* v = as<T>(o))
        SWITCH
        CASE(IdentType) {
            fn(v->arguments);
        }
        CASE(Call) {
            fn(v->expr_type);
            fn(v->callee);
            fn(v->arguments);
        }
        CASE(Binary) {
            fn(v->expr_type);
            fn(v->left);
            fn(v->right);
        }
        CASE(Unary) {
            fn(v->expr_type);
            fn(v->target);
        }
        CASE(Cond) {
            fn(v->expr_type);
            fn(v->cond);
            fn(v->then);
            fn(v->els);
        }
        CASE(If) {
            fn(v->expr_type);
            fn(v->cond);
            fn(v->block);
            fn(v->els);
        }
        CASE(For) {
            fn(v->block);
        }
        CASE(Fmt) {
            fn(v->scope);
        }
        CASE(TmpVar) {
            fn(v->expr_type);
        }
        CASE(IndentScope) {
            for (auto& f : v->elements) {
                fn(f);
            }
        }
        CASE(Program) {
            for (auto& f : v->elements) {
                fn(f);
            }
        }
        CASE(Field) {
            fn(v->field_type);
        }
        CASE(MemberAccess) {
            fn(v->expr_type);
            fn(v->target);
        }
#undef SWITCH
#undef CASE
    }
}  // namespace ast
