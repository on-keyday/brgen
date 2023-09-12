/*license*/
#pragma once
#include "ast.h"
#include "node/traverse.h"
#include <map>
#include <json/json_export.h>
#include <json/iterator.h>

#include "ast.h"
#include "../common/error.h"
#include <helper/transform.h>
#include <optional>
#include <helper/expected_op.h>

namespace brgen::ast {

    using JSON = utils::json::JSON;

    constexpr auto bool_to_error(const char* err) {
        return [=](bool res) -> either::expected<void, const char*> {
            if (!res) {
                return either::unexpected{err};
            }
            return {};
        };
    }

    struct JSONConverter {
        Debug obj;

       private:
        std::map<std::shared_ptr<Node>, size_t> node_index;
        std::map<std::shared_ptr<Scope>, size_t> scope_index;
        std::vector<std::shared_ptr<Node>> nodes;
        std::vector<std::shared_ptr<Scope>> scopes;

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

       public:
        void clear() {
            node_index.clear();
            nodes.clear();
            obj.out().clear();
        }

        void encode(const std::shared_ptr<Node>& root_node) {
            clear();
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

        inline either::expected<const JSON*, const char*> json_at(const JSON& js, auto&& key) {
            const char* err = nullptr;
            auto res = js.at(key, &err);
            if (!res) {
                return either::unexpected{err};
            }
            return res;
        }

        static constexpr auto empty_node = either::empty_value<std::shared_ptr<Node>>();

        constexpr auto json_to_loc_error(lexer::Loc loc, auto key, std::optional<NodeType> type = std::nullopt) {
            return [=](const char* m) {
                auto k = [&] {
                    if constexpr (std::is_integral_v<decltype(key)>) {
                        return nums(key);
                    }
                    else {
                        return key;
                    }
                };
                if (type) {
                    return error(loc, m, " at ", k(), " of ", node_type_to_string(*type));
                }
                else {
                    return error(loc, m, " at ", k());
                }
            };
        }

        constexpr auto js_as_number(auto& n) {
            return [&](auto js) {
                js->force_as_number(n);
            };
        }

        auto parse_loc(lexer::Loc& loc) {
            return [&](auto js) {
                json_at(*js, "pos").transform([&](auto js) {
                    json_at(*js, "begin").transform(js_as_number(loc.pos.begin));
                    json_at(*js, "end").transform(js_as_number(loc.pos.end));
                });
                json_at(*js, "file").transform(js_as_number(loc.file));
            };
        }

        auto get_string(lexer::Loc loc, auto key) {
            return [=](auto js) {
                std::string tmp;
                return bool_to_error("as_string returned false")(js->as_string(tmp)) & [&] { return tmp; } | json_to_loc_error(loc, key);
            };
        }

        auto get_number(lexer::Loc loc, auto key) {
            return [=](auto js) {
                size_t tmp = 0;
                return bool_to_error("as_number returned false")(js->as_number(tmp)) & [&] { return tmp; } | json_to_loc_error(loc, key);
            };
        }

        inline result<std::shared_ptr<Node>> parse_single_node(const JSON& js) {
            lexer::Loc loc;
            json_at(js, "loc") & parse_loc(loc);
            auto type = (json_at(js, "node_type") | json_to_loc_error(loc, "node_type")) &
                        [](const JSON* js) {
                            return js->force_as_string<std::string>();
                        } &
                        [=](const std::string& str) {
                            return string_to_node_type(str) | json_to_loc_error(loc, "node_type");
                        };
            if (!type) {
                return type & empty_node;
            }
            NodeType node_type = *type;

            auto loc_error = [&](const char* key) {
                return json_to_loc_error(loc, key, node_type);
            };
            auto get_key = [&](const char* key) {
                return json_at(js, key) | loc_error(key);
            };

            auto parse_node = [&](const char* key, auto& target) -> result<void> {
                using T = std::remove_reference_t<decltype(target)>;
                auto res = get_key(key);
                if constexpr (std::is_same_v<T, std::string>) {
                    return (res & [&](auto js) { return js->as_string(target); })
                        .and_then([&](bool ok) {
                            return bool_to_error("as_string returned false")(ok) | loc_error(key);
                        });
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    return (res & [&](auto js) { return js->as_bool(target); })
                        .and_then([&](bool ok) {
                            return bool_to_error("as_bool returned false")(ok) | loc_error(key);
                        });
                }
                else if constexpr (std::is_same_v<T, size_t>) {
                    return res.and_then(get_number(loc, key)).transform(either::assign_to(target));
                }
                else if constexpr (std::is_same_v<T, lexer::Loc>) {
                    return res & parse_loc(target);
                }
                else if constexpr (std::is_same_v<T, BinaryOp>) {
                    return (res & get_string(loc, key))
                        .and_then([&](std::string&& s) -> result<void> {
                            if (!bin_op(s.c_str()).transform([&](BinaryOp op) { target = op;return true; })) {
                                return unexpect(error(loc, s, " cannot convert to binary operator"));
                            }
                            return {};
                        });
                }
                else if constexpr (std::is_same_v<T, UnaryOp>) {
                    return (res & get_string(loc, key))
                        .and_then([&](std::string&& s) -> result<void> {
                            if (!unary_op(s.c_str()).transform([&](UnaryOp op) { target = op;return true; })) {
                                return unexpect(error(loc, s, " cannot convert to unary operator"));
                            }
                            return {};
                        });
                }
                else if constexpr (std::is_same_v<T, IdentUsage>) {
                    return (res & get_string(loc, key))
                        .and_then([&](std::string&& s) -> result<void> {
                            if (!ident_usage(s.c_str()).transform([&](IdentUsage op) { target = op;return true; })) {
                                return unexpect(error(loc, s, " cannot convert to unary operator"));
                            }
                            return {};
                        });
                }
                else if constexpr (utils::helper::is_template_instance_of<T, std::shared_ptr> ||
                                   utils::helper::is_template_instance_of<T, std::weak_ptr> ||
                                   std::is_same_v<T, node_list> ||
                                   std::is_same_v<T, const NodeType>) {
                    // nothing to do
                    return {};
                }
                else {
                    static_assert(std::is_same_v<T, std::string>);
                    return {};
                }
            };
            std::shared_ptr<Node> node;
            result<void> err;
            get_node(node_type, [&](auto n) {
                using NodeT = typename decltype(n)::node;
                if constexpr (!decltype(n)::is_abs) {
                    auto rep = std::make_shared<NodeT>();
                    rep->dump([&](auto key, auto& target) {
                        err = err & [&] { return parse_node(key, target); };
                    });
                    if (!err) {
                        return;
                    }
                    node = std::move(rep);
                }
            });
            if (!err) {
                return err & empty_node;
            }
            return node;
        }

        static constexpr auto must_be_array = [](auto js) {
            constexpr auto f = bool_to_error("must be array");
            return f(js->is_array()) & [&] { return js; };
        };

        inline result<void> link_nodes(const JSON& node_s) {
            for (size_t i = 0; i < nodes.size(); i++) {
                result<void> res;
                visit(nodes[i], [&](auto&& f) {
                    f->dump([&](auto key, auto& value) {
                        if (!res) {
                            return;  // already error
                        }
                        using T = std::decay_t<decltype(value)>;
                        constexpr auto is_shared_or_weak = utils::helper::is_template_instance_of<T, std::shared_ptr> ||
                                                           utils::helper::is_template_instance_of<T, std::weak_ptr>;
                        auto& val = node_s[i];
                        auto check_index = [&](auto&& index) {
                            if (!index) {
                                res = index & empty_value<void>();
                                return false;
                            }
                            if constexpr (std::is_same_v<T, std::shared_ptr<Scope>>) {
                                if (*index >= scopes.size()) {
                                    res = unexpect(error(f->loc, "missing reference; index out of range, index", nums(*index), "but range are 0-", nums(scopes.size() - 1)));
                                    return false;
                                }
                            }
                            else {
                                if (*index >= nodes.size()) {
                                    res = unexpect(error(f->loc, "missing reference; index out of range, index", nums(*index), "but range are 0-", nums(nodes.size() - 1)));
                                    return false;
                                }
                                if constexpr (is_shared_or_weak) {
                                    using P = typename utils::helper::template_of_t<T>::template param_at<0>;
                                    if constexpr (!std::is_same_v<Node, P>) {
                                        if (!ast::as<P>(nodes[*index])) {
                                            res = unexpect(error(f->loc, "missing reference: expect node ", node_type_to_string(P::node_type_tag), " but found ", node_type_to_string(nodes[*index]->node_type)));
                                            return false;
                                        }
                                    }
                                }
                            }
                            return true;
                        };
                        auto data = json_at(val, key);
                        if (!data) {
                            res = (data | json_to_loc_error(f->loc, key)) & empty_value<void>();
                            return;
                        }
                        if ((*data)->is_null()) {
                            return;  // skip
                        }
                        auto obj = data | empty_value<LocationError>();
                        if constexpr (std::is_same_v<T, std::shared_ptr<Scope>>) {
                            auto index = obj.and_then(get_number(f->loc, key));
                            if (!check_index(index)) {
                                return;
                            }
                            value = scopes[*index];
                        }
                        else if constexpr (is_shared_or_weak) {
                            auto index = obj.and_then(get_number(f->loc, key));
                            if (!check_index(index)) {
                                return;
                            }
                            using P = typename utils::helper::template_of_t<T>::template param_at<0>;
                            value = cast_to<P>(nodes[*index]);
                        }
                        else if constexpr (std::is_same_v<T, node_list>) {
                            auto arr = std::move(obj);
                            if (!(*arr)->is_array()) {
                                res = unexpect(json_to_loc_error(f->loc, key)("must be array"));
                                return;
                            }
                            for (auto& js : utils::json::as_array(**arr)) {
                                auto index = get_number(f->loc, key)(&js);
                                if (!check_index(index)) {
                                    return;
                                }
                                value.push_back(nodes[*index]);
                            }
                        }
                    });
                });
                if (!res) {
                    return res;
                }
            }
            return {};
        }

        inline result<std::shared_ptr<Node>> decode(const JSON& js) {
            clear();
            if (js.is_null()) {
                return nullptr;
            }
            auto node_s = json_at(js, "node") & must_be_array;
            if (!node_s) {
                return (node_s & empty_node) | json_to_loc_error({}, "node");
            }
            auto scope_list = json_at(js, "scope") & must_be_array;
            if (!scope_list) {
                return (scope_list & empty_node) | json_to_loc_error({}, "scope");
            }
            for (auto& node : utils::json::as_array(**node_s)) {
                auto result = parse_single_node(node);
                if (!result) {
                    return result;
                }
                nodes.push_back(std::move(*result));
            }
            if (nodes.size() == 0) {
                return unexpect(error({}, "least 1 element required for node"));
            }
            for (auto& scope : utils::json::as_array(**scope_list)) {
                scopes.push_back(std::make_shared<Scope>());  // currently only add scope; no link collect
            }

            auto res = link_nodes(**node_s);
            if (!res) {
                return res & empty_node;
            }

            for (size_t i = 0; i < scopes.size(); i++) {
                auto& val = (**scope_list)[i];
                auto get_scope = [&](const char* key) {
                    return (json_at(val, key) | json_to_loc_error({}, key)) & get_number({}, key) &
                               [&](size_t i) -> result<std::shared_ptr<Scope>> {
                        if (i >= scopes.size()) {
                            return unexpect(error({}, "index out of range"));
                        }
                        return scopes[i];
                    };
                };
                auto b = get_scope("branch");
                if (!b) {
                    return b & empty_node;
                }
                scopes[i]->branch = std::move(*b);
                auto n = get_scope("next");
                if (!n) {
                    return n & empty_node;
                }
                scopes[i]->next = std::move(*n);
                auto ident = (json_at(val, "ident") & must_be_array) | json_to_loc_error({}, "ident");
                if (!ident) {
                    return ident & empty_node;
                }
                for (auto& id : utils::json::as_array(**ident)) {
                    auto index = get_number({}, "ident")(&id);
                    if (*index >= nodes.size()) {
                        return unexpect(error({}, "index out of range"));
                    }
                    auto val = nodes[*index];
                    if (as<Field>(val)) {
                        scopes[i]->push(cast_to<Field>(val));
                    }
                    else if (as<Format>(val)) {
                        scopes[i]->push(cast_to<Format>(val));
                    }
                    else if (as<Ident>(val)) {
                        scopes[i]->push(cast_to<Ident>(val));
                    }
                    else {
                        return unexpect(error({}, "expect field,format,ident but found ", node_type_to_string(val->node_type)));
                    }
                }
            }
            return nodes[0];
        }
    };

}  // namespace brgen::ast
