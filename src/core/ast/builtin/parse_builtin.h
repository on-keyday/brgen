/*license*/
#pragma once
#include "../node/builtin.h"
#include <json/json_export.h>
#include <core/common/expected.h>

namespace brgen::ast {
    struct BuiltinError {
        std::string msg;
    };

    auto builtin_error(auto&&... msg) {
        return futils::helper::either::unexpected{BuiltinError{brgen::concat(msg...)}};
    }

    expected<std::shared_ptr<BuiltinObject>, BuiltinError> parse(const futils::json::JSON& js) {
        if (!js.is_object()) {
            to_string(js.kind());
            return builtin_error("expected object, got ", to_string(js.kind()));
        }
        auto t = js.at("type");
        if (!t) {
            return builtin_error("expect type field which is a string of either 'object', 'field', or 'function'");
        }
        if (!t->is_string()) {
            return builtin_error("expect type field which is a string of either 'object', 'field', or 'function'");
        }
        auto type = t->force_as_string<std::string>();
        if (type == "object") {
            auto m = js.at("members");
            if (!m) {
                return builtin_error("expect members field which is an array of either 'object', 'field', or 'function'");
            }
            if (!m->is_array()) {
                return builtin_error("expect members field which is an array of either 'object', 'field', or 'function'");
            }
            auto obj = std::make_shared<BuiltinObject>();
        }
    }
}  // namespace brgen::ast
