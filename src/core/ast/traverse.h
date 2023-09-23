/*license*/
#pragma once
#include "ast.h"
#include <helper/template_instance.h>

namespace brgen::ast {

    template <class NodeT>
    struct node_embed {
        using node = NodeT;

        static constexpr auto is_abs = !std::is_default_constructible_v<NodeT>;
    };

    constexpr void get_node(NodeType type, auto&& fn) {
#define SWITCH \
    switch (type) {
#define END_SWITCH() }
#define CASE(T)              \
    case T::node_type_tag: { \
        fn(node_embed<T>{}); \
        return;              \
    }
        SWITCH
        CASE(Program)
        // literal
        CASE(Literal)
        CASE(IntLiteral)
        CASE(BoolLiteral)
        CASE(StrLiteral)

        CASE(Input)
        CASE(Output)
        CASE(Config)

        // expr
        CASE(Expr)

        CASE(Binary)
        CASE(Unary)
        CASE(Cond)
        CASE(Ident)
        CASE(Call)
        CASE(If)
        CASE(MemberAccess)
        CASE(Paren)
        CASE(Index)
        CASE(Match)
        CASE(Range)

        CASE(TmpVar)
        CASE(BlockExpr)
        CASE(Import)

        // stmt
        CASE(Stmt)
        CASE(Loop)
        CASE(IndentScope)
        CASE(MatchBranch)
        CASE(Return)
        CASE(Break)
        CASE(Continue)

        CASE(Assert)
        CASE(ImplicitYield)

        // member
        CASE(Member)

        CASE(Field)
        CASE(Format)
        CASE(Function)

        // type
        CASE(Type)

        CASE(IntType)
        CASE(IdentType)
        CASE(IntLiteralType)
        CASE(StrLiteralType)
        CASE(VoidType)
        CASE(BoolType)
        CASE(ArrayType)
        CASE(FunctionType)
        CASE(StructType)
        CASE(UnionType)

        END_SWITCH()
#undef SWITCH
#undef CASE
#undef END_SWITCH
    }

    void visit(auto&& t, auto&& fn) {
        Node* o = std::to_address(t);
        if (!o) {
            fn(cast_to<Node>(std::forward<decltype(t)>(t)));
            return;
        }
        get_node(o->node_type, [&](auto node) {
            fn(cast_to<typename decltype(node)::node>(std::forward<decltype(t)>(t)));
        });
    }

    void traverse(auto&& t, auto&& fn) {
        Node* o = std::to_address(t);
#define SWITCH   \
    if (false) { \
    }
#define CASE(T) else if (T* v = as<T>(o))
        if (!o) {
            return;  // no traverse
        }
        visit(o, [&](auto f) {
            f->dump([&](auto key, auto& value) {
                if constexpr (utils::helper::is_template_instance_of<std::decay_t<decltype(value)>, std::shared_ptr>) {
                    using T = typename utils::helper::template_of_t<std::decay_t<decltype(value)>>::template param_at<0>;
                    if constexpr (std::is_base_of_v<Node, T>) {
                        fn(value);
                    }
                }
                else if constexpr (utils::helper::is_template_instance_of<std::decay_t<decltype(value)>, std::list> ||
                                   utils::helper::is_template_instance_of<std::decay_t<decltype(value)>, std::vector>) {
                    using T = typename utils::helper::template_instance_of_t<std::decay_t<decltype(*value.begin())>, std::shared_ptr>;
                    if constexpr (T::value) {
                        if constexpr (std::is_base_of_v<Node, typename T::template param_at<0>>) {
                            for (auto& v : value) {
                                fn(v);
                            }
                        }
                    }
                }
            });
        });
    }
}  // namespace brgen::ast
