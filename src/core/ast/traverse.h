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
        case NodeType::node: {
            assert(false);
            return;
        }
            CASE(Program)
            CASE(Comment)
            CASE(CommentGroup)
            CASE(FieldArgument)

            // literal
            CASE(Literal)
            CASE(IntLiteral)
            CASE(BoolLiteral)
            CASE(StrLiteral)
            CASE(RegexLiteral)
            CASE(CharLiteral)
            CASE(TypeLiteral)
            CASE(SpecialLiteral)

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

            CASE(Identity)
            // translated
            CASE(TmpVar)
            CASE(Import)
            CASE(Cast)
            CASE(Available)
            CASE(SpecifyOrder)
            CASE(ExplicitError)
            CASE(IOOperation)
            CASE(OrCond)

            CASE(BadExpr)

            // stmt
            CASE(Stmt)

            CASE(Loop)
            CASE(ScopedStatement)
            CASE(IndentBlock)
            CASE(MatchBranch)
            CASE(UnionCandidate)
            CASE(Return)
            CASE(Break)
            CASE(Continue)

            CASE(Assert)
            CASE(ImplicitYield)
            CASE(Metadata)

            // member
            CASE(Member)

            CASE(Field)
            CASE(Format)
            CASE(State)
            CASE(Enum)
            CASE(EnumMember)
            CASE(Function)

            // type
            CASE(Type)

            CASE(IntType)
            CASE(FloatType)
            CASE(IdentType)
            CASE(IntLiteralType)
            CASE(StrLiteralType)
            CASE(RegexLiteralType)
            CASE(VoidType)
            CASE(BoolType)
            CASE(ArrayType)
            CASE(FunctionType)
            CASE(StructType)
            CASE(StructUnionType)
            CASE(UnionType)
            CASE(RangeType)
            CASE(EnumType)
            CASE(MetaType)
            CASE(OptionalType)
            CASE(GenericType)

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
        if (!o) {
            return;  // no traverse
        }
        visit(o, [&](auto f) {
            f->dump([&](auto key, auto& value) {
                if constexpr (futils::helper::is_template_instance_of<std::decay_t<decltype(value)>, std::shared_ptr>) {
                    using T = typename futils::helper::template_of_t<std::decay_t<decltype(value)>>::template param_at<0>;
                    if constexpr (std::is_base_of_v<Node, T>) {
                        fn(value);
                    }
                }
                else if constexpr (futils::helper::is_template_instance_of<std::decay_t<decltype(value)>, std::vector>) {
                    using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(*value.begin())>, std::shared_ptr>;
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

    template <class B>
    struct FieldWrapper {
        B& base;

        constexpr auto operator()(std::string_view key, auto&& value) const {
            using P = futils::helper::template_of_t<std::decay_t<decltype(value)>>;
            if constexpr (std::is_same_v<decltype(value), scope_ptr&>) {
                if (key == "global_scope") {
                    base(key, value);
                }
            }
            else if constexpr (P::value) {
                if constexpr (futils::helper::is_template_instance_of<std::decay_t<decltype(value)>, std::weak_ptr>) {
                    // ignore at here
                }
                else {
                    using P2 = futils::helper::template_of_t<typename P::template param_at<0>>;
                    if constexpr (P2::value) {
                        if constexpr (futils::helper::is_template_instance_of<typename P2::instance, std::weak_ptr>) {
                            // ignore at here
                        }
                        else {
                            base(key, value);
                        }
                    }
                    else {
                        base(key, value);
                    }
                }
            }
            else {
                base(key, value);
            }
        }
    };

    template <class T>
        requires std::is_base_of_v<Node, T>
    void as_json(T& t, JSONWriter& buf) {
        auto field = buf.object();
        if constexpr (std::is_default_constructible_v<T>) {
            t.dump(FieldWrapper<decltype(field)>{field});
        }
        else {
            visit(static_cast<Node*>(&t), [&](auto f) { f->dump(FieldWrapper<decltype(field)>{field}); });
        }
    }
}  // namespace brgen::ast
