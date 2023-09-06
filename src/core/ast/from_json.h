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
    constexpr auto to_node() {
        return [](auto&& p) -> either::expected<std::shared_ptr<NodeT>, LocationError> {
            if (as<NodeT>(p)) {
                return std::static_pointer_cast<NodeT>();
            }
        };
    }

    inline result<std::shared_ptr<Node>> from_json(const JSON& js) {
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
        switch (node) {
            case NodeType::ident: {
                auto ident = std::make_shared<Ident>();
                json_at(js, "expr_type")
                    .transform_error(json_to_loc_error(loc, "expr_type", NodeType::ident))
                    .and_then([&](auto js) {
                        return from_json(*js);
                    });
            }
        }
    }
}  // namespace brgen::ast
