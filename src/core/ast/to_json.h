/*license*/
#pragma once
#include "ast.h"
#include "node/traverse.h"
#include <map>

namespace brgen::ast {
    struct SymbolMap {
        std::map<std::shared_ptr<Node>, size_t> index;
        std::vector<std::shared_ptr<Node>> nodes;
        Debug obj;

        void collect(const std::shared_ptr<Node>& node) {
            auto found = index.find(node);
            if (found != index.end()) {
                return;  // skip; already visited
            }
            nodes.push_back(node);
            index[node] = nodes.size() - 1;
            visit(node, [&](auto&& f) {
                f->dump([&]<class T>(std::string_view key, T& value) {
                    if (key == "scope") {
                        ;
                    }
                    if constexpr (utils::helper::is_template_instance_of<T, std::shared_ptr>) {
                        using type = typename utils::helper::template_instance_of_t<T, std::shared_ptr>::template param_at<0>;
                        if constexpr (std::is_base_of_v<Node, type>) {
                            collect(value);
                        }
                    }
                    else if constexpr (std::is_same_v<T, node_list>) {
                        for (auto& element : value) {
                            collect(element);
                        }
                    }
                });
            });
        }

        void encode(const std::shared_ptr<Node>& root_node) {
            index.clear();
            nodes.clear();
            collect(root_node);
        }
    };

}  // namespace brgen::ast
