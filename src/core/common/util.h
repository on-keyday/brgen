/*license*/
#pragma once

#include <string>
#include <number/to_string.h>
#include <escape/escape.h>
#include <optional>

namespace brgen {
    auto nums(auto v, int radix = 10) {
        return futils::number::to_string<std::string>(v, radix);
    }

    using futils::strutil::append, futils::strutil::appends;

    inline std::optional<std::string> unescape(std::string_view str_lit) {
        std::string mid;
        if (!futils::escape::unescape_str(str_lit.substr(1, str_lit.size() - 2), mid)) {
            return std::nullopt;
        }
        return mid;
    }

    inline std::optional<size_t> unescape_count(std::string_view str_lit) {
        futils::helper::CountPushBacker pb;
        futils::helper::IPushBacker<uint8_t> ipb{pb};
        if (!futils::escape::unescape_str(str_lit.substr(1, str_lit.size() - 2), ipb)) {
            return std::nullopt;
        }
        return pb.count;
    }

    inline std::string escape(std::string_view str_lit) {
        return futils::escape::escape_str<std::string>(str_lit, futils::escape::EscapeFlag::utf16 | futils::escape::EscapeFlag::hex);
    }

    inline std::string concat(auto&&... msg) {
        std::string buf;
        appends(buf, msg...);
        return buf;
    }

}  // namespace brgen
