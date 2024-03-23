/*license*/
#pragma once
#include "expr.h"
#include "statement.h"
#include <variant>
#include <helper/defer.h>

namespace brgen::ast {

    struct Scope {
        std::weak_ptr<Scope> prev;
        std::vector<std::weak_ptr<Ident>> objects;
        std::shared_ptr<Scope> branch;
        std::shared_ptr<Scope> next;
        std::weak_ptr<Node> owner;
        bool branch_root = false;

        std::optional<std::shared_ptr<Ident>> lookup_current(auto&& fn, ast::Ident* self = nullptr) {
            bool myself_appear = !self;
            for (auto it = objects.rbegin(); it != objects.rend(); it++) {
                auto& val = *it;
                auto obj = val.lock();
                if (!myself_appear) {
                    if (obj.get() == self) {
                        myself_appear = true;
                    }
                    continue;
                }
                if (fn(obj)) {
                    return obj;
                }
            }
            if (auto got = prev.lock()) {
                if (got->branch.get() == this) {
                    return std::nullopt;
                }
                return got->template lookup_local(fn);
            }
            return std::nullopt;
        }

        std::optional<std::shared_ptr<Ident>> lookup_local(auto&& fn, ast::Ident* self = nullptr) {
            if (auto locked = owner.lock(); locked && locked.get()->node_type == NodeType::program) {
                return std::nullopt;
            }
            bool myself_appear = !self;
            for (auto it = objects.rbegin(); it != objects.rend(); it++) {
                auto& val = *it;
                auto obj = val.lock();
                if (!myself_appear) {
                    if (obj.get() == self) {
                        myself_appear = true;
                    }
                    continue;
                }
                if (fn(obj)) {
                    return obj;
                }
            }
            if (auto got = prev.lock()) {
                return got->template lookup_local(fn);
            }
            return std::nullopt;
        }

        std::optional<std::shared_ptr<Ident>> lookup_global(auto&& fn) {
            for (auto& val : objects) {
                auto obj = val.lock();
                if (fn(obj)) {
                    return obj;
                }
            }
            if (next) {
                return next->template lookup_global(fn);
            }
            return std::nullopt;
        }

        std::shared_ptr<Scope> next_scope() const {
            return next;
        }

        std::shared_ptr<Scope> branch_scope() const {
            return branch;
        }

        void push(const std::shared_ptr<Ident>& obj) {
            objects.push_back({std::forward<decltype(obj)>(obj)});
        }

        void as_json(JSONWriter& d) {
            auto field = d.array();
            auto add_field = [&](auto& self) {
                for (auto& object_w : self.objects) {
                    auto object = object_w.lock();
                    if (!object) {
                        continue;
                    }
                    field([&] {
                        auto field_ = d.object();
                        auto usage = object->usage;
                        auto ident = object->ident;
                        sdebugf(usage);
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
        std::vector<std::shared_ptr<Scope>> prev_branch_end;

        void maybe_init() {
            if (!root) {
                root = std::make_shared<Scope>();
                current = root;
                root->branch_root = true;
            }
            if (current->branch && !current->next) {
                current->next = std::make_shared<Scope>();
                current->next->prev = current;
                current->next->owner = current->owner;
                current = current->next;
                for (auto& prev : prev_branch_end) {
                    prev->next = current;
                }
                prev_branch_end.clear();
            }
        }

       public:
        auto enter_branch() {
            maybe_init();
            current->branch = std::make_shared<Scope>();
            current->branch->prev = current;
            auto d = current;
            current = current->branch;
            current->branch_root = true;
            return futils::helper::defer([this, d] {
                prev_branch_end.push_back(std::move(current));
                current = d;
            });
        }

        std::shared_ptr<Scope> current_scope() {
            maybe_init();
            return current;
        }
    };
}  // namespace brgen::ast
