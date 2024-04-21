/*license*/
#pragma once
#include "send.h"
#include <core/ast/file.h>
#include <core/ast/json.h>

std::shared_ptr<brgen::ast::Node> load_json(std::uint64_t id, auto&& name, auto&& input) {
    auto js = futils::json::parse<futils::json::JSON>(input);
    if (js.is_undef()) {
        send_error_and_end(id, "cannot parse json file: ", name);
        return nullptr;
    }
    brgen::ast::AstFile file;
    if (!futils::json::convert_from_json(js, file)) {
        send_error_and_end(id, "cannot convert json file to ast: ", name);
        return nullptr;
    }
    if (!file.ast) {
        send_error_and_end(id, "cannot convert json file to ast: ast is null: ", name);
        return nullptr;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*file.ast);
    if (!res) {
        send_error_and_end(id, "cannot decode json file: ", res.error().locations[0].msg);
        return nullptr;
    }
    if (!*res) {
        send_error_and_end(id, "cannot decode json file: ast is null: ", name);
        return nullptr;
    }
    return *res;
}