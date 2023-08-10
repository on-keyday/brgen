/*license*/
#pragma once
#include "util.h"
#include <escape/escape.h>

namespace brgen {
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
            append(buf, "null");
        }

        void boolean(bool v) {
            append(buf, v ? "true" : "false");
        }
    };

}  // namespace brgen