/*license*/
#pragma once
#include <memory>
#include <helper/defer.h>
#include <optional>
#include "debug.h"

namespace brgen {

    template <class T>
    struct StackFrame {
        template <class V>
        friend struct Stack;

       private:
        std::weak_ptr<StackFrame> prev;
        std::shared_ptr<StackFrame> branch;
        std::shared_ptr<StackFrame> next;

       public:
        T current;

        template <class V>
        std::optional<V> lookup(auto&& fn) {
            std::optional<V> f = fn(current);
            if (f) {
                return f;
            }
            if (auto got = prev.lock()) {
                return got->template lookup<V>(fn);
            }
            return std::nullopt;
        }

        template <class V>
        std::optional<V> lookup_first(auto&& fn) {
            if (auto got = prev.lock()) {
                if (auto found = got->template lookup_first<V>(fn)) {
                    return found;
                }
            }
            return fn(current);
        }

        std::shared_ptr<StackFrame> next_frame() const {
            return next;
        }

        std::shared_ptr<StackFrame> branch_frame() const {
            return branch;
        }

        void as_json(Debug& debug) {}
    };

    template <class T>
    struct Stack {
       private:
        std::shared_ptr<StackFrame<T>> root;
        std::shared_ptr<StackFrame<T>> current;

        void maybe_init() {
            if (!root) {
                root = std::make_shared<StackFrame<T>>();
                current = root;
            }
            if (current->branch && !current->next) {
                current->next = std::make_shared<StackFrame<T>>();
                current->next->prev = current;
                current = current->next;
            }
        }

       public:
        void enter_branch() {
            maybe_init();
            current->branch = std::make_shared<StackFrame<T>>();
            current->branch->prev = current;
            current = current->branch;
        }

        void leave_branch() {
            current = current->prev.lock();
        }

        std::shared_ptr<StackFrame<T>> current_frame() {
            maybe_init();
            return current;
        }
    };

}  // namespace brgen
