/*license*/
#pragma once

#include <json/json_export.h>

#include "ast.h"
#include "translated.h"
#include "../common/error.h"
#include <helper/transform.h>

namespace brgen::ast {
    using JSON = utils::json::JSON;
    inline either::expected<const JSON*, const char*> json_access(const JSON& js, auto&& key) {
        const char* err = nullptr;
        auto res = js.at(key, &err);
        if (!res) {
            return either::unexpected{err};
        }
        return res;
    }

    constexpr auto empty_node = either::empty_value<std::shared_ptr<Node>>();

    inline result<std::shared_ptr<Node>> from_json(const JSON& js) {
        auto type = json_access(js, "node_type")
                        .transform_error([](const char* m) { return error({}, m); })
                        .transform([](const JSON* js) {
                            return js->force_as_string<std::string>();
                        });
        if (!type) {
            return type.transform(empty_node);
        }
        lexer::Loc loc_info;
        json_access(js, "loc").transform([&](auto loc) {
            json_access(*loc, "pos").transform([&](auto js) {
                json_access(*js, "begin").transform([&](auto js) { js->force_as_number(loc_info.pos.begin); });
                json_access(*js, "end").transform([&](auto js) { js->force_as_number(loc_info.pos.end); });
            });
            json_access(*loc, "file").transform([&](auto js) { js->force_as_number(loc_info.file); });
        });
    }
}  // namespace brgen::ast
