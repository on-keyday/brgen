/*license*/
#pragma once
#include <memory>
#include <helper/defer.h>
#include <optional>

namespace ast {

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
        }

       public:
        void enter_branch() {
            maybe_init();
            current->branch = std::make_shared<StackFrame<T>>();
            current->branch->prev = current;
            current->next = std::make_shared<StackFrame<T>>();
            current->next->prev = current;
            current = current->branch;
        }

        void leave_branch() {
            current = current->prev.lock();
            current = current->next;
        }

        std::shared_ptr<StackFrame<T>> current_frame() {
            maybe_init();
            return current;
        }
    };

}  // namespace ast
