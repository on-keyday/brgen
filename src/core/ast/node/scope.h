/*license*/
#pragma once
#include "expr.h"
#include "statement.h"
#include <variant>

namespace brgen::ast {
    struct Object {
        std::variant<std::weak_ptr<Ident>, std::weak_ptr<Field>, std::weak_ptr<Format>, std::weak_ptr<Function>> object;

        std::optional<std::string> ident() {
            switch (object.index()) {
                case 0: {
                    auto g = std::get<std::weak_ptr<Ident>>(object).lock();
                    if (!g) {
                        return std::nullopt;
                    }
                    return g->ident;
                }
                case 1: {
                    auto g = std::get<std::weak_ptr<Field>>(object).lock();
                    if (!g || !g->ident) {
                        return std::nullopt;
                    }
                    return g->ident->ident;
                }
                case 2: {
                    auto g = std::get<std::weak_ptr<Format>>(object).lock();
                    if (!g || !g->ident) {
                        return std::nullopt;
                    }
                    return g->ident->ident;
                }
                case 3: {
                    auto g = std::get<std::weak_ptr<Function>>(object).lock();
                    if (!g || !g->ident) {
                        return std::nullopt;
                    }
                    return g->ident->ident;
                }
            }
            return std::nullopt;
        }

        NodeType node_type() {
            switch (object.index()) {
                case 0:
                    return NodeType::ident;
                case 1:
                    return NodeType::field;
                case 2:
                    return NodeType::format;
                case 3:
                    return NodeType::function;
            }
            return NodeType::program;
        }

        void visit(auto&& fn) {
            std::visit(
                [&](auto& o) {
                    fn(o.lock());
                },
                object);
        }
    };

    struct Scope {
        std::weak_ptr<Scope> prev;
        std::list<Object> objects;
        std::shared_ptr<Scope> branch;
        std::shared_ptr<Scope> next;

        template <class V>
        std::optional<std::shared_ptr<V>> lookup_backward(auto&& fn) {
            for (auto it = objects.rbegin(); it != objects.rend(); it++) {
                auto& val = *it;
                if (std::holds_alternative<std::weak_ptr<V>>(val.object)) {
                    auto obj = std::get<std::weak_ptr<V>>(val.object).lock();
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
        std::optional<std::shared_ptr<V>> lookup_forward(auto&& fn) {
            if (auto got = prev.lock()) {
                if (auto found = got->template lookup_forward<V>(fn)) {
                    return found;
                }
            }
            for (auto& val : objects) {
                if (std::holds_alternative<std::weak_ptr<V>>(val.object)) {
                    auto obj = std::get<std::weak_ptr<V>>(val.object).lock();
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

        void as_json(Debug& d) {
            auto field = d.array();
            auto add_field = [&](auto& self) {
                for (auto& object : self.objects) {
                    field([&] {
                        auto field = d.object();
                        auto ident = object.ident();
                        const auto node_type = object.node_type();
                        field(sdebugf(node_type));
                        field(sdebugf(ident));
                    });
                }
                if (self.branch) {
                    field([&] {
                        self.branch->as_json(d);
                    });
                }
            };
            add_field(*this);
            for (auto p = next; p; p = p->next) {
                add_field(*p);
            }
        }
    };
}  // namespace brgen::ast
