/*license*/
#pragma once
#include "ast.h"
#include "translated.h"
#include <helper/template_instance.h>

namespace brgen::ast {
    template <class U>
    constexpr auto cast_to(auto&& t) {
        using T = std::decay_t<decltype(t)>;
        if constexpr (utils::helper::is_template_instance_of<T, std::shared_ptr>) {
            using V = typename utils::helper::template_instance_of_t<T, std::shared_ptr>::template param_at<0>;
            return std::static_pointer_cast<V>(std::forward<decltype(t)>(t));
        }
        else {
            return static_cast<T*>(t);
        }
    }

    void visit(auto&& t, auto&& fn) {
        Node* o = std::to_address(t);
#define SWITCH   \
    if (false) { \
    }
#define CASE(T)                                       \
    else if (as<T>(o)) {                              \
        fn(cast_to<T>(std::forward<decltype(t)>(t))); \
    }
        SWITCH
        CASE(Program)

        CASE(IntLiteral)
        CASE(BoolLiteral)
        CASE(StrLiteral)

        CASE(Input)
        CASE(Output)

        CASE(Binary)
        CASE(Unary)
        CASE(Cond)
        CASE(Ident)
        CASE(Call)
        CASE(If)
        CASE(MemberAccess)
        CASE(Paren)

        CASE(TmpVar)
        CASE(BlockExpr)

        CASE(Stmt)

        CASE(For)
        CASE(Field)
        CASE(Fmt)
        CASE(IndentScope)

        CASE(Assert)
        CASE(ImplicitReturn)

        CASE(IntType)
        CASE(IdentType)
        CASE(IntLiteralType)
        CASE(StrLiteralType)
        CASE(VoidType)
        CASE(BoolType)
        CASE(ArrayType)

#undef SWITCH
#undef CASE
    }

    void traverse(auto&& t, auto&& fn) {
        Node* o = std::to_address(t);
#define SWITCH   \
    if (false) { \
    }
#define CASE(T) else if (T* v = as<T>(o))
        SWITCH
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
        CASE(Paren) {
            fn(v->expr);
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
            fn(v->ident);
            fn(v->field_type);
            fn(v->arguments);
        }
        CASE(MemberAccess) {
            fn(v->expr_type);
            fn(v->target);
        }
        CASE(Assert) {
            fn(v->cond);
        }
        CASE(ArrayType) {
            fn(v->base_type);
            fn(v->length);
        }
        CASE(ImplicitReturn) {
            fn(v->expr);
        }
#undef SWITCH
#undef CASE
    }
}  // namespace brgen::ast
