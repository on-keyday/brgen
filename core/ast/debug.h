/*license*/
#pragma once
#include <escape/escape.h>
#include <string>

namespace ast {
    auto nums(auto v, int radix = 10) {
        return utils::number::to_string<std::string>(v, radix);
    }

    using utils::strutil::append, utils::strutil::appends;

    struct Debug {
        std::string buf;
        void object(auto&& fn) {
            append(buf, "{");
            bool comma = false;
            fn([&](std::string_view name, auto&& value) {
                if (comma) {
                    append(buf, ",");
                }
                appends(buf, "\"", name, "\": ");
                value(*this);
                comma = true;
            });
            append(buf, "}");
        }

        void array(auto&& fn) {
            append(buf, "[");
            bool comma = false;
            fn([&](auto&& value) {
                if (comma) {
                    append(buf, ",");
                }
                value(*this);
                comma = true;
            });
            append(buf, "]");
        }

        void number(std::int64_t value) {
            append(buf, nums(value));
        }

        void string(std::string_view s) {
            appends(buf, "\"", utils::escape::escape_str<std::string>(s, utils::escape::EscapeFlag::all, utils::escape::json_set()), "\"");
        }

        void null() {
            appends(buf, "null");
        }

        void boolean(bool v) {
            append(buf, v ? "true" : "false");
        }
    };

}  // namespace ast