/*license*/
#pragma once
#include "../ast/node/base.h"
#include "../ast/ast.h"

namespace brgen::middle {
    struct NodeReplacer {
       private:
        void* ptr;
        void (*assign)(void*, std::shared_ptr<ast::Node>&&);
        std::shared_ptr<ast::Node> (*to_node_)(void*);

        template <class T>
        static void do_assign(void* p, std::shared_ptr<ast::Node>&& node) {
            using N = typename futils::helper::template_of_t<T>::template param_at<0>;
            *static_cast<T*>(p) = ast::cast_to<N>(std::move(node));
        }

        template <class T>
        static std::shared_ptr<ast::Node> do_to_node(void* p) {
            return *static_cast<T*>(p);
        }

       public:
        // copy and move
        NodeReplacer(const NodeReplacer&) = default;
        NodeReplacer(NodeReplacer&&) = default;

        template <class T, helper_disable_self(NodeReplacer, T)>
        constexpr NodeReplacer(T& t)
            : ptr(std::addressof(t)), assign(do_assign<T>), to_node_(do_to_node<T>) {}

        constexpr void replace(std::shared_ptr<ast::Node>&& node) {
            assign(ptr, std::move(node));
        }

        std::shared_ptr<ast::Node> to_node() {
            return to_node_(ptr);
        }
    };
}  // namespace brgen::middle
