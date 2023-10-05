/*license*/
#pragma once
#include "expr.h"
#include "statement.h"
#include <variant>
#include <helper/defer.h>

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
        bool is_global = false;

        template <class V>
        std::optional<std::shared_ptr<V>> lookup_local(auto&& fn) {
            if (is_global) {
                return std::nullopt;
            }
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
                return got->template lookup_local<V>(fn);
            }
            return std::nullopt;
        }

        template <class V>
        std::optional<std::shared_ptr<V>> lookup_global(auto&& fn) {
            for (auto& val : objects) {
                if (std::holds_alternative<std::weak_ptr<V>>(val.object)) {
                    auto obj = std::get<std::weak_ptr<V>>(val.object).lock();
                    if (fn(obj)) {
                        return obj;
                    }
                }
            }
            if (next) {
                return next->template lookup_global<V>(fn);
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

        void as_json(JSONWriter& d) {
            auto field = d.array();
            auto add_field = [&](auto& self) {
                for (auto& object : self.objects) {
                    field([&] {
                        auto field = d.object();
                        auto ident = object.ident();
                        const auto node_type = object.node_type();
                        sdebugf(node_type);
                        sdebugf(ident);
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

    struct ScopeStack {
       private:
        std::shared_ptr<Scope> root;
        std::shared_ptr<Scope> current;

        void maybe_init() {
            if (!root) {
                root = std::make_shared<Scope>();
                current = root;
                current->is_global = true;
            }
            if (current->branch && !current->next) {
                current->next = std::make_shared<Scope>();
                current->next->prev = current;
                current->next->is_global = current->is_global;
                current = current->next;
            }
        }

       public:
        auto enter_branch() {
            maybe_init();
            current->branch = std::make_shared<Scope>();
            current->branch->prev = current;
            current->branch->is_global = false;
            auto d = current;
            current = current->branch;
            return utils::helper::defer([this, d] {
                current = d;
            });
        }

        std::shared_ptr<Scope> current_scope() {
            maybe_init();
            return current;
        }
    };
}  // namespace brgen::ast
