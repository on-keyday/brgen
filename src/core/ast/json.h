/*license*/
#pragma once
#include "ast.h"
#include "traverse.h"
#include <unordered_map>
#include <json/json_export.h>
#include <json/iterator.h>

#include "ast.h"
#include "../common/error.h"
#include <helper/transform.h>
#include <optional>
#include <helper/expected_op.h>

namespace brgen::ast {

    using JSON = futils::json::JSON;

    constexpr auto bool_to_error(const char* err) {
        return [=](bool res) -> either::expected<void, const char*> {
            if (!res) {
                return either::unexpected{err};
            }
            return {};
        };
    }

    struct JSONConverter {
        JSONWriter obj;

       private:
        std::unordered_map<std::shared_ptr<Node>, size_t> node_index;
        std::unordered_map<std::shared_ptr<Scope>, size_t> scope_index;
        std::vector<std::shared_ptr<Node>> nodes;
        std::vector<std::shared_ptr<Scope>> scopes;

        void collect(const std::shared_ptr<Scope>& scope) {
            auto collect_scope = [&](const std::shared_ptr<Scope>& scope) {
                auto found = scope_index.find(scope);
                if (found != scope_index.end()) {
                    return;  // skip; already visited
                }
                scopes.push_back(scope);
                scope_index[scope] = scopes.size() - 1;
            };
            std::vector<std::shared_ptr<Scope>> stack;
            stack.push_back(std::move(scope));
            while (!stack.empty()) {
                auto v = stack.back();
                stack.pop_back();
                collect_scope(v);
                if (v->next) {
                    if (v->next->prev.lock() == v) {
                        stack.push_back(v->next);
                    }
                }
                if (v->branch) {
                    stack.push_back(v->branch);
                }
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
                    if constexpr (futils::helper::is_template_instance_of<T, std::shared_ptr>) {
                        using type = typename futils::helper::template_instance_of_t<T, std::shared_ptr>::template param_at<0>;
                        if constexpr (std::is_base_of_v<Node, type>) {
                            collect(value);
                        }
                        else {
                            if (key == "global_scope") {
                                collect(value);
                            }
                        }
                    }
                    else if constexpr (futils::helper::is_template_instance_of<T, std::vector>) {
                        using type = typename futils::helper::template_of_t<T>::template param_at<0>;
                        if constexpr (futils::helper::is_template_instance_of<type, std::shared_ptr>) {
                            for (auto& element : value) {
                                collect(element);
                            }
                        }
                    }
                });
            });
        }

       public:
        void clear() {
            node_index.clear();
            nodes.clear();
            scope_index.clear();
            scopes.clear();
            obj.out().clear();
        }

        void encode(const std::shared_ptr<Node>& root_node) {
            clear();  // clear internal
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
                            node->dump(field);
                            auto dump_f = [&] {
                                auto field = obj.object();
                                f->dump([&]<class T>(std::string_view key, T& value) {
                                    if constexpr (futils::helper::is_template_instance_of<T, std::shared_ptr>) {
                                        using type = typename futils::helper::template_instance_of_t<T, std::shared_ptr>::template param_at<0>;
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
                                    else if constexpr (futils::helper::is_template_instance_of<T, std::weak_ptr>) {
                                        using type = typename futils::helper::template_instance_of_t<T, std::weak_ptr>::template param_at<0>;
                                        if constexpr (std::is_base_of_v<Node, type>) {
                                            find_and_replace_node(value.lock(), [&](auto&& val) {
                                                field(key, val);
                                            });
                                        }
                                    }
                                    else if constexpr (futils::helper::is_template_instance_of<T, std::vector>) {
                                        field(key, [&] {
                                            auto field = obj.array();
                                            for (auto& element : value) {
                                                if constexpr (futils::helper::is_template_instance_of<std::decay_t<decltype(element)>, std::weak_ptr>) {
                                                    find_and_replace_node(element.lock(), field);
                                                }
                                                else {
                                                    find_and_replace_node(element, field);
                                                }
                                            }
                                        });
                                    }
                                    else if constexpr (futils::helper::is_template_instance_of<T, std::map>) {
                                        field(key, [&] {
                                            auto field = obj.object();
                                            for (auto& f : value) {
                                                find_and_replace_node(f.second, [&](auto&& val) {
                                                    field(f.first, val);
                                                });
                                            }
                                        });
                                    }
                                    else if constexpr (std::is_same_v<T, const NodeType>) {
                                        // nothing to do
                                    }
                                    else if constexpr (std::is_same_v<T, lexer::Loc>) {
                                        if (key != "loc") {
                                            field(key, value);
                                        }
                                    }
                                    else {
                                        field(key, value);
                                    }
                                });
                            };
                            field("body", dump_f);
                        });
                    });
                }
            };
            auto encode_scope = [&] {
                auto field = obj.array();
                for (auto& scope : scopes) {
                    field([&] {
                        auto field = obj.object();
                        find_and_replace_node(scope->owner.lock(), [&](auto val) {
                            field("owner", val);
                        });
                        field("ident", [&] {
                            auto field = obj.array();
                            for (auto& object : scope->objects) {
                                find_and_replace_node(object.lock(), field);
                            }
                        });
                        find_and_replace_scope(scope->branch, [&](auto val) {
                            field("branch", val);
                        });
                        find_and_replace_scope(scope->next, [&](auto val) {
                            field("next", val);
                        });
                        find_and_replace_scope(scope->prev.lock(), [&](auto val) {
                            field("prev", val);
                        });
                        field("branch_root", scope->branch_root);
                    });
                }
            };
            field("node_count", nodes.size());  // for json parser helper
            field("node", encode_node);
            field("scope_count", scopes.size());  // for json parser helper
            field("scope", encode_scope);
        }

       private:
        static either::expected<const JSON*, const char*> json_at(const JSON& js, auto&& key) {
            const char* err = nullptr;
            auto res = js.at(key, &err);
            if (!res) {
                return either::unexpected{err};
            }
            return res;
        }

        static constexpr auto empty_node = either::empty_value<std::shared_ptr<Node>>();

        static constexpr auto json_to_loc_error(lexer::Loc loc, auto key, std::optional<NodeType> type = std::nullopt) {
            return [=](const char* message) {
                auto k = [&] {
                    if constexpr (std::is_integral_v<decltype(key)>) {
                        return nums(key);
                    }
                    else {
                        return key;
                    }
                };
                return (type) ? error(loc, message, " at ", k(), " of ", node_type_to_string(*type))
                              : error(loc, message, " at ", k());
            };
        }

        static auto parse_loc(lexer::Loc& loc) {
            return [&](auto js) {
                json_at(*js, "pos").transform([&](auto js) {
                    json_at(*js, "begin").transform([&](auto js) { js->force_as_number(loc.pos.begin); });
                    json_at(*js, "end").transform([&](auto js) { js->force_as_number(loc.pos.end); });
                });
                json_at(*js, "file").transform([&](auto js) { js->force_as_number(loc.file); });
                json_at(*js, "line").transform([&](auto js) { js->force_as_number(loc.line); });
                json_at(*js, "col").transform([&](auto js) { js->force_as_number(loc.col); });
            };
        }

        static auto get_string(lexer::Loc loc, auto key) {
            return [=](auto js) {
                std::string tmp;
                return bool_to_error("as_string returned false")(js->as_string(tmp)) & [&] { return tmp; } | json_to_loc_error(loc, key);
            };
        }

        static auto get_number(lexer::Loc loc, auto key) {
            return [=](auto js) {
                size_t tmp = 0;
                return bool_to_error("as_number returned false")(js->as_number(tmp)) & [&] { return tmp; } | json_to_loc_error(loc, key);
            };
        }

        static result<NodeType> get_node_type(const JSON& js, lexer::Loc loc) {
            return (json_at(js, "node_type") | json_to_loc_error(loc, "node_type")) &
                   [](const JSON* js) {
                       return js->force_as_string<std::string>();
                   } &
                   [=](const std::string& str) {
                       return string_to_node_type(str) | json_to_loc_error(loc, "node_type");
                   };
        }

        static result<void> parse_non_node_field(const JSON& js, NodeType node_type, lexer::Loc loc,
                                                 const char* key, auto& target) {
            auto loc_error = [&]() {
                return json_to_loc_error(loc, key, node_type);
            };
            auto get_key = [&]() {
                return json_at(js, key) | loc_error();
            };

            using T = std::remove_reference_t<decltype(target)>;
            auto res = get_key();
            if constexpr (std::is_same_v<T, std::string>) {
                return (res & [&](auto js) { return js->as_string(target); })
                    .and_then([&](bool ok) {
                        return bool_to_error("as_string returned false")(ok) | loc_error();
                    });
            }
            else if constexpr (std::is_same_v<T, bool>) {
                return (res & [&](auto js) { return js->as_bool(target); })
                    .and_then([&](bool ok) {
                        return bool_to_error("as_bool returned false")(ok) | loc_error();
                    });
            }
            else if constexpr (std::is_same_v<T, size_t>) {
                return res.and_then(get_number(loc, key)).transform(either::assign_to(target));
            }
            else if constexpr (std::is_same_v<T, std::optional<size_t>>) {
                return res.and_then([&](auto js) -> expected<std::optional<size_t>, LocationError> {
                              static_assert(std::is_same_v<std::decay_t<decltype(js)>, const JSON*>);
                              if (js->is_null()) {
                                  return std::optional<size_t>{};
                              }
                              return get_number(loc, key)(js).transform([](auto&& v) -> std::optional<size_t> { return v; });
                          })
                    .transform(either::assign_to(target));
            }
            else if constexpr (std::is_same_v<T, lexer::Loc>) {
                if (std::string_view(key) == "loc") {
                    return {};  // skip
                }
                return res & parse_loc(target);
            }
            else if constexpr (std::is_enum_v<T> && !std::is_same_v<T, const NodeType>) {
                if constexpr (is_bit_flag<T>()) {
                    return (res & get_number(loc, key)).and_then([&](size_t v) -> result<void> {
                        if (auto res = from_json<T>(v); !res) {
                            return unexpect(error(loc, nums(v), " cannot convert to bit flag ", enum_type_name<T>()));
                        }
                        else {
                            target = *res;
                        }
                        return {};
                    });
                }
                else {
                    return (res & get_string(loc, key)).and_then([&](std::string&& s) -> result<void> {
                        if (auto res = from_string<T>(s.c_str()); !res) {
                            return unexpect(error(loc, s, "cannot convert to enum ", enum_type_name<T>()));
                        }
                        else {
                            target = *res;
                        }
                        return {};
                    });
                }
            }
            else if constexpr (futils::helper::is_template_instance_of<T, std::shared_ptr> ||
                               futils::helper::is_template_instance_of<T, std::weak_ptr> ||
                               futils::helper::is_template_instance_of<T, std::vector> ||
                               futils::helper::is_template_instance_of<T, std::map> ||
                               std::is_same_v<T, const NodeType>) {
                // nothing to do
                return {};
            }
            else {
                static_assert(std::is_same_v<T, std::string>);
                return {};
            }
        }

        static result<std::shared_ptr<Node>> parse_single_node(const JSON& js) {
            lexer::Loc loc;
            json_at(js, "loc") & parse_loc(loc);
            auto type = get_node_type(js, loc);
            if (!type) {
                return type & empty_node;
            }
            NodeType node_type = *type;
            std::shared_ptr<Node> node;
            result<void> err;
            auto body = json_at(js, "body") | json_to_loc_error(loc, "body", node_type);
            if (!body) {
                return body & empty_node;
            }
            get_node(node_type, [&](auto n) {
                using NodeT = typename decltype(n)::node;
                if constexpr (!decltype(n)::is_abs) {
                    auto rep = std::make_shared<NodeT>();
                    rep->loc = loc;
                    rep->dump([&](auto key, auto& target) {
                        err = err & [&]() -> result<void> { return parse_non_node_field(**body, node_type, loc, key, target); };
                    });
                    if (!err) {
                        return;
                    }
                    node = std::move(rep);
                }
                else {
                    err = unexpect(error(loc, "abstract node_type is not allowed"));
                }
            });
            if (!err) {
                return err & empty_node;
            }
            return node;
        }

        static constexpr auto must_be_array = [](auto js) {
            auto f = bool_to_error("must be array");
            return f(js->is_array()) & [&] { return js; };
        };

        result<void> link_nodes(const JSON& node_s) {
            for (size_t i = 0; i < nodes.size(); i++) {
                result<void> res;
                visit(nodes[i], [&](auto&& f) {
                    f->dump([&](auto key, auto& value) {
                        if (!res) {
                            return;  // already error
                        }
                        if (std::string_view(key) == "node_type" ||
                            std::string_view(key) == "loc") {
                            return;  // skip
                        }
                        using T = std::decay_t<decltype(value)>;
                        constexpr auto is_shared_or_weak = futils::helper::is_template_instance_of<T, std::shared_ptr> ||
                                                           futils::helper::is_template_instance_of<T, std::weak_ptr>;
                        constexpr auto is_list = futils::helper::is_template_instance_of<T, std::vector>;
                        constexpr auto is_map = futils::helper::is_template_instance_of<T, std::map>;
                        auto& val = node_s[i]["body"];
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
                                    using P = typename futils::helper::template_of_t<T>::template param_at<0>;
                                    if constexpr (!std::is_same_v<Node, P>) {
                                        if (!ast::as<P>(nodes[*index])) {
                                            res = unexpect(error(f->loc, "missing reference: expect node ", node_type_to_string(P::node_type_tag), " but found ", node_type_to_string(nodes[*index]->node_type)));
                                            return false;
                                        }
                                    }
                                }
                                else if constexpr (is_list) {
                                    using P1 = typename futils::helper::template_of_t<T>::template param_at<0>;
                                    using P2 = typename futils::helper::template_of_t<P1>::template param_at<0>;
                                    if constexpr (!std::is_same_v<Node, P2>) {
                                        if (!ast::as<P2>(nodes[*index])) {
                                            res = unexpect(error(f->loc, "missing reference: expect node ", node_type_to_string(P2::node_type_tag), " but found ", node_type_to_string(nodes[*index]->node_type)));
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
                            using P = typename futils::helper::template_of_t<T>::template param_at<0>;
                            value = cast_to<P>(nodes[*index]);
                        }
                        else if constexpr (is_list) {
                            auto arr = std::move(obj);
                            if (!(*arr)->is_array()) {
                                res = unexpect(json_to_loc_error(f->loc, key)("must be array"));
                                return;
                            }
                            for (auto& js : futils::json::as_array(**arr)) {
                                auto index = get_number(f->loc, key)(&js);
                                if (!check_index(index)) {
                                    return;
                                }
                                using P = typename futils::helper::template_of_t<std::decay_t<decltype(value.front())>>::template param_at<0>;
                                value.push_back(cast_to<P>(nodes[*index]));
                            }
                        }
                        else if constexpr (is_map) {
                            if (!(*obj)->is_object()) {
                                res = unexpect(json_to_loc_error(f->loc, key)("must be object"));
                                return;
                            }
                            for (auto& [k, v] : futils::json::as_object(**obj)) {
                                auto index = get_number(f->loc, key)(&v);
                                if (!check_index(index)) {
                                    return;
                                }
                                using P = typename futils::helper::template_of_t<std::decay_t<decltype(value.begin()->second)>>::template param_at<0>;
                                value[k] = cast_to<P>(nodes[*index]);
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

        result<void> link_scopes(const JSON& scope_list) {
            for (size_t i = 0; i < scopes.size(); i++) {
                auto& val = scope_list[i];
                auto get_scope = [&](const char* key) -> result<std::shared_ptr<Scope>> {
                    auto res = json_at(val, key);
                    if (!res) {
                        return res & empty_value<std::shared_ptr<Scope>>() | json_to_loc_error({}, key);
                    }
                    if ((*res)->is_null()) {
                        return nullptr;
                    }
                    return (res | empty_value<LocationError>()) & get_number({}, key) &
                               [&](size_t i) -> result<std::shared_ptr<Scope>> {
                        if (i >= scopes.size()) {
                            return unexpect(error({}, "index out of range"));
                        }
                        return scopes[i];
                    };
                };
                auto branch_root = json_at(val, "branch_root");
                if (!branch_root) {
                    return branch_root & empty_value<void>() | json_to_loc_error({}, "branch_root");
                }
                if (!(*branch_root)->as_bool(scopes[i]->branch_root)) {
                    return unexpect(error({}, "branch_root must be true"));
                }
                auto b = get_scope("branch");
                if (!b) {
                    return b & empty_value<void>();
                }
                scopes[i]->branch = std::move(*b);
                auto n = get_scope("next");
                if (!n) {
                    return n & empty_value<void>();
                }
                scopes[i]->next = std::move(*n);
                auto p = get_scope("prev");
                if (!p) {
                    return p & empty_value<void>();
                }
                scopes[i]->prev = std::move(*p);
                auto ident = (json_at(val, "ident") & must_be_array) | json_to_loc_error({}, "ident");
                if (!ident) {
                    return ident & empty_value<void>();
                }
                for (auto& id : futils::json::as_array(**ident)) {
                    auto index = get_number({}, "ident")(&id);
                    if (!index) {
                        return index & empty_value<void>();
                    }
                    if (*index >= nodes.size()) {
                        return unexpect(error({}, "index out of range"));
                    }
                    auto val = nodes[*index];
                    if (as<Ident>(val)) {
                        scopes[i]->push(cast_to<Ident>(val));
                    }
                    else {
                        return unexpect(error({}, "expect ident but found ", node_type_to_string(val->node_type)));
                    }
                }
                auto owner = json_at(val, "owner");
                if (!owner) {
                    return owner & empty_value<void>() | json_to_loc_error({}, "owner");
                }
                if (!(*owner)->is_null()) {
                    auto index = get_number({}, "owner")(*owner);
                    if (!index) {
                        return index & empty_value<void>();
                    }
                    if (*index >= nodes.size()) {
                        return unexpect(error({}, "index out of range"));
                    }
                    scopes[i]->owner = nodes[*index];
                }
            }
            return {};
        }

        result<void> parse_nodes(const JSON& node_s) {
            for (auto& node : futils::json::as_array(node_s)) {
                auto result = parse_single_node(node);
                if (!result) {
                    return result & empty_value<void>();
                }
                nodes.push_back(std::move(*result));
            }
            if (nodes.size() == 0) {
                return unexpect(error({}, "least 1 element required for node"));
            }
            return {};
        }

       public:
        result<std::shared_ptr<Node>> decode(const JSON& js) {
            clear();  // clear internal cache

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

            auto res = parse_nodes(**node_s);

            if (!res) {
                return res & empty_node;
            }

            for (auto& scope : futils::json::as_array(**scope_list)) {
                // currently only add scope; no link branch and next
                scopes.push_back(std::make_shared<Scope>());
            }

            res = link_nodes(**node_s);
            if (!res) {
                return res & empty_node;
            }

            res = link_scopes(**scope_list);
            if (!res) {
                return res & empty_node;
            }

            return nodes[0];
        }
    };

}  // namespace brgen::ast
