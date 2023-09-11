/*license*/
#pragma once
#include "ast.h"
#include "node/traverse.h"
#include <map>

namespace brgen::ast {
    struct SymbolMap {
        std::map<std::shared_ptr<Node>, size_t> node_index;
        std::map<std::shared_ptr<Scope>, size_t> scope_index;
        std::vector<std::shared_ptr<Node>> nodes;
        std::vector<std::shared_ptr<Scope>> scopes;
        Debug obj;

        void collect(const std::shared_ptr<Scope>& scope) {
            auto found = scope_index.find(scope);
            if (found != scope_index.end()) {
                return;  // skip; already visited
            }
            scopes.push_back(scope);
            scope_index[scope] = scopes.size() - 1;
            if (scope->branch) {
                collect(scope->branch);
            }
            if (scope->next) {
                collect(scope->next);
            }
        }

        void collect(const std::shared_ptr<Node>& node) {
            if (!node) {
                return;  // skip null node
            }
            auto found = node_index.find(node);
            if (found != node_index.end()) {
                return;  // skip; already visited
            }
            nodes.push_back(node);
            node_index[node] = nodes.size() - 1;
            visit(node, [&](auto&& f) {
                f->dump([&]<class T>(std::string_view key, T& value) {
                    if (key == "base") {
                        ;
                    }
                    if constexpr (utils::helper::is_template_instance_of<T, std::shared_ptr>) {
                        using type = typename utils::helper::template_instance_of_t<T, std::shared_ptr>::template param_at<0>;
                        if constexpr (std::is_base_of_v<Node, type>) {
                            collect(value);
                        }
                        else {
                            if (key == "global_scope") {
                                collect(value);
                            }
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
            node_index.clear();
            nodes.clear();
            obj.out().clear();
            collect(root_node);
            auto field = obj.object();
            auto find_and_replace_node = [this](auto&& node, auto&& field) {
                auto it = node_index.find(node);
                if (it == node_index.end()) {
                    field(nullptr);
                }
                else {
                    field(it->second);
                }
            };
            auto find_and_replace_scope = [this](auto&& node, auto&& field) {
                auto it = scope_index.find(node);
                if (it == scope_index.end()) {
                    field(nullptr);
                }
                else {
                    field(it->second);
                }
            };
            auto encode_node = [&] {
                auto field = obj.array();
                for (auto node : nodes) {
                    visit(node, [&](auto&& f) {
                        field([&] {
                            auto field = obj.object();
                            f->dump([&]<class T>(std::string_view key, T& value) {
                                if constexpr (utils::helper::is_template_instance_of<T, std::shared_ptr>) {
                                    using type = typename utils::helper::template_instance_of_t<T, std::shared_ptr>::template param_at<0>;
                                    if constexpr (std::is_base_of_v<Node, type>) {
                                        find_and_replace_node(value, [&](auto&& val) {
                                            field(key, val);
                                        });
                                    }
                                    else if constexpr (std::is_same_v<Scope, type>) {
                                        find_and_replace_scope(value, [&](auto&& val) {
                                            field(key, val);
                                        });
                                    }
                                }
                                else if constexpr (utils::helper::is_template_instance_of<T, std::weak_ptr>) {
                                    using type = typename utils::helper::template_instance_of_t<T, std::weak_ptr>::template param_at<0>;
                                    if constexpr (std::is_base_of_v<Node, type>) {
                                        find_and_replace_node(value.lock(), [&](auto&& val) {
                                            field(key, val);
                                        });
                                    }
                                }
                                else if constexpr (std::is_same_v<T, node_list>) {
                                    field(key, [&] {
                                        auto field = obj.array();
                                        for (auto& element : value) {
                                            find_and_replace_node(element, field);
                                        }
                                    });
                                }
                                else {
                                    field(key, value);
                                }
                            });
                        });
                    });
                }
            };
            auto encode_scope = [&] {
                auto field = obj.array();
                for (auto& scope : scopes) {
                    field([&] {
                        auto field = obj.object();
                        field("ident", [&] {
                            auto field = obj.array();
                            for (auto& object : scope->objects) {
                                object.visit([&](auto&& node) {
                                    find_and_replace_node(node, field);
                                });
                            }
                        });
                        find_and_replace_scope(scope->branch, [&](auto val) {
                            field("branch", val);
                        });
                        find_and_replace_scope(scope->next, [&](auto val) {
                            field("next", val);
                        });
                    });
                }
            };
            field("node", encode_node);
            field("scope", encode_scope);
        }
    };

}  // namespace brgen::ast
