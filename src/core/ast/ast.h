/*license*/
#pragma once
#include "node/base.h"
#include "node/expr.h"
#include "node/statement.h"
#include "node/type.h"
#include "node/translated.h"
#include "node/scope.h"

namespace brgen::ast {

    template <class T>
    constexpr T* as(auto&& t) {
        Node* v = std::to_address(t);
        if constexpr (std::is_same_v<T, Node>) {
            return v;
        }
        else if constexpr (std::is_same_v<T, Expr> || std::is_same_v<T, Type> ||
                           std::is_same_v<T, Stmt> || std::is_same_v<T, Literal> ||
                           std::is_same_v<T, Member>) {
            if (v && (int(v->node_type) & int(T::node_type_tag)) == int(T::node_type_tag)) {
                return static_cast<T*>(v);
            }
        }
        else {
            if (v && v->node_type == T::node_type_tag) {
                return static_cast<T*>(v);
            }
        }
        return nullptr;
    }

    template <class U>
    constexpr auto cast_to(auto&& t) {
        assert(as<U>(t) != nullptr);
        using T = std::decay_t<decltype(t)>;
        if constexpr (futils::helper::is_template_instance_of<T, std::shared_ptr>) {
            using V = typename futils::helper::template_instance_of_t<T, std::shared_ptr>::template param_at<0>;
            return std::static_pointer_cast<U>(std::forward<decltype(t)>(t));
        }
        else {
            return static_cast<U*>(t);
        }
    }

}  // namespace brgen::ast
