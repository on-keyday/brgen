/*license*/
#pragma once
#include "../ast.h"

namespace brgen::ast::tool {
    std::optional<std::string> lookup_str_metadata(std::string_view name, std::vector<std::weak_ptr<Metadata>>& md) {
        for (auto& m : md) {
            auto p = m.lock();
            if (p && p->name == name && p->values.size() == 1) {
                if (auto s = ast::as<ast::StrLiteral>(p->values[0])) {
                    auto input = s->value.substr(1, s->value.size() - 2);
                    return futils::escape::unescape_str<std::string>(input);
                }
            }
        }
        return std::nullopt;
    }
}  // namespace brgen::ast::tool
