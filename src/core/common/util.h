/*license*/
#pragma once

#include <string>
#include <number/to_string.h>
#include <escape/escape.h>
#include <optional>

namespace brgen {
    auto nums(auto v, int radix = 10) {
        return utils::number::to_string<std::string>(v, radix);
    }

    using utils::strutil::append, utils::strutil::appends;

    inline std::optional<std::string> unescape(std::string_view str_lit) {
        std::string mid;
        if (!utils::escape::unescape_str(str_lit.substr(1, str_lit.size() - 2), mid)) {
            return std::nullopt;
        }
        return mid;
    }

}  // namespace brgen
