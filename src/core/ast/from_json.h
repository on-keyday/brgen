/*license*/
#pragma once

#include <json/json_export.h>

#include "ast.h"
#include "translated.h"
#include "../common/error.h"
#include <helper/transform.h>
#include <optional>

namespace brgen::ast {
    using JSON = utils::json::JSON;
    inline either::expected<const JSON*, const char*> json_at(const JSON& js, auto&& key) {
        const char* err = nullptr;
        auto res = js.at(key, &err);
        if (!res) {
            return either::unexpected{err};
        }
        return res;
    }

    constexpr auto empty_node = either::empty_value<std::shared_ptr<Node>>();

    constexpr auto json_to_loc_error(lexer::Loc loc, const char* key, std::optional<NodeType> type = std::nullopt) {
        return [=](const char* m) {
            if (type) {
                return error(loc, m, " at ", key, " of ", node_type_to_string(*type));
            }
            else {
                return error(loc, m, " at ", key);
            }
        };
    }

    constexpr auto js_as_number(auto& n) {
        return [&](auto js) {
            js->force_as_number(n);
        };
    }

    template <class NodeT>
    constexpr auto to_node(lexer::Loc loc) {
        return [=](auto&& p) -> either::expected<std::shared_ptr<NodeT>, LocationError> {
            if (!p) {
                return nullptr;
            }
            if (as<NodeT>(p)) {
                return std::static_pointer_cast<NodeT>(p);
            }
            return either::unexpected{error(loc, "expect ", node_type_to_string(NodeT::node_type), " but found ", node_type_to_string(p->type))};
        };
    }

    constexpr auto bool_to_error(bool res, const char* err) -> either::expected<void, const char*> {
        if (!res) {
            return either::unexpected{err};
        }
        return {};
    }

    inline result<std::shared_ptr<Node>> from_json(const JSON& js) {
        if (js.is_null()) {
            return nullptr;
        }
        lexer::Loc loc;
        auto parse_loc = [&](auto js) {
            json_at(*js, "pos").transform([&](auto js) {
                json_at(*js, "begin").transform(js_as_number(loc.pos.begin));
                json_at(*js, "end").transform(js_as_number(loc.pos.end));
            });
            json_at(*js, "file").transform(js_as_number(loc.file));
        };

        json_at(js, "loc").transform(parse_loc);
        auto type = json_at(js, "node_type")
                        .transform_error(json_to_loc_error(loc, "node_type"))
                        .transform([](const JSON* js) {
                            return js->force_as_string<std::string>();
                        })
                        .and_then([=](const std::string& str) {
                            return string_to_node_type(str).transform_error(json_to_loc_error(loc, "node_type"));
                        });
        if (!type) {
            return type.transform(empty_node);
        }
        NodeType node = *type;

        auto loc_error = [&](const char* key) {
            return json_to_loc_error(loc, key, node);
        };
        auto get_key = [&](const char* key) {
            return json_at(js, key)
                .transform_error(loc_error(key));
        };
        auto parse_node = [&](const char* key, auto& target) {
            using T = std::remove_reference_t<decltype(target)>;
            auto res = get_key(key);
            if constexpr (utils::helper::is_template_instance_of<T, std::shared_ptr>) {
                using tio = utils::helper::template_instance_of_t<T, std::shared_ptr>;
                using NodeT = typename tio::template param_at<0>;
                if constexpr (std::is_base_of_v<Node, NodeT>) {
                    return res
                        .and_then([&](auto js) {
                            return from_json(*js);
                        })
                        .and_then(to_node<NodeT>(loc))
                        .transform(either::assign_to(target));
                }
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                return res.and_then([&](auto js) {
                    return bool_to_error(js->as_string(target), "as_string returned false")
                        .transform_error(loc_error(key));
                });
            }
        };
        auto parse_keys = [&](const char* key, auto& target, auto&&... other) {
            auto f = [&](auto&& f, const char* key, auto& target, auto&&... other) -> expected<void, LocationError> {
                auto res = parse_node(key, target);
                if (!res) {
                    return res;
                }
                if constexpr (sizeof...(other)) {
                    return f(f, other...);
                }
                return expected<void, LocationError>{};
            };
            return f(f, key, target, other...);
        };

        switch (node) {
            case NodeType::ident: {
                auto ident = std::make_shared<Ident>();
                parse_keys("expr_type", ident->expr_type,
                           "ident", ident->ident);
            }
            default: {
                return either::unexpected{error(loc, "unimplemented")};
            }
        }
    }
}  // namespace brgen::ast
