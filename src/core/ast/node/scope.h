/*license*/
#pragma once
#include "expr.h"
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
                return got->lookup_current(fn);
            }
            return std::nullopt;
        }

        bool is_type_ident(const std::shared_ptr<Ident>& ident) {
            return ident && (ident->usage == IdentUsage::define_format ||
                             ident->usage == IdentUsage::define_enum ||
                             ident->usage == IdentUsage::define_state);
        }

        std::optional<std::shared_ptr<Ident>> lookup_backward(auto&& fn, ast::Ident* self = nullptr, bool may_forward = false, bool only_type_allowed = false) {
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
                if (only_type_allowed) {
                    if (!is_type_ident(obj)) {
                        continue;
                    }
                }
                if (fn(obj, may_forward)) {
                    return obj;
                }
            }

            if (auto got = prev.lock()) {
                if (this->branch_root) {
                    auto owner_locked = this->owner.lock();
                    if (owner_locked &&
                        (owner_locked.get()->node_type == NodeType::format ||
                         owner_locked.get()->node_type == NodeType::state ||
                         owner_locked.get()->node_type == NodeType::enum_)) {
                        only_type_allowed = true;
                    }
                }
                auto result = got->lookup_backward(fn, nullptr, may_forward || this->branch_root, only_type_allowed);
                if (result) {
                    return result;
                }
            }
            if (auto got = next) {
                auto result = got->lookup_forward(fn, true);
                if (result) {
                    return result;
                }
            }
            return std::nullopt;
        }

        std::optional<std::shared_ptr<Ident>> lookup_forward(auto&& fn, bool only_type_allowed = false) {
            for (auto& val : objects) {
                auto obj = val.lock();
                if (only_type_allowed) {
                    if (!is_type_ident(obj)) {
                        continue;
                    }
                }
                if (fn(obj, true)) {
                    return obj;
                }
            }
            if (next) {
                return next->lookup_forward(fn);
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
