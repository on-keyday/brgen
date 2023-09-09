/*license*/
#pragma once
#include "expr.h"
#include "statement.h"
#include <variant>

namespace brgen::ast {
    struct Object {
        std::variant<std::shared_ptr<Ident>, std::shared_ptr<Field>, std::shared_ptr<Format>> object;
    };

    struct Scope {
        std::weak_ptr<Scope> prev;
        std::list<Object> objects;
        std::shared_ptr<Scope> branch;
        std::shared_ptr<Scope> next;

        template <class V>
        std::optional<V> lookup_backward(auto&& fn) {
            for (auto& val : std::views::reverse(objects)) {
                if (std::holds_alternative<V>(val.object)) {
                    auto obj = std::get<V>(val.object);
                    if (fn(obj)) {
                        return obj;
                    }
                }
            }
            if (auto got = prev.lock()) {
                return got->template lookup_backward<V>(fn);
            }
            return std::nullopt;
        }

        template <class V>
        std::optional<V> lookup_forward(auto&& fn) {
            if (auto got = prev.lock()) {
                if (auto found = got->template lookup_forward<V>(fn)) {
                    return found;
                }
            }
            for (auto& val : objects) {
                if (std::holds_alternative<V>(val.object)) {
                    auto obj = std::get<V>(val.object);
                    if (fn(obj)) {
                        return obj;
                    }
                }
            }
            return std::nullopt;
        }

        std::shared_ptr<Scope> next_scope() const {
            return next;
        }

        std::shared_ptr<Scope> branch_scope() const {
            return branch;
        }

        void push(auto&& obj) {
            objects.push_back({std::forward<decltype(obj)>(obj)});
        }

        void as_json(Debug&) {}
    };
}  // namespace brgen::ast
