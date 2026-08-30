/*license*/
#pragma once
#include <format>
#include <source_location>
#include <string>
#include "../lexer/token.h"
#include <code/src_location.h>
#include "../common/util.h"
#include "expected.h"
#include "helper/expected.h"
#include <vector>
#include <algorithm>

namespace brgen {
    struct SourceEntry {
        using error_buffer_type = std::string;
        std::string msg;
        std::string file;
        lexer::Loc loc;
        std::string src;
        bool warn = false;

        std::string to_string() {
            std::string buf;
            error(buf);
            return buf;
        }

        void error(auto&& buf) const {
            appends(buf, warn ? "warning: " : "error: ");
            omit_error(buf);
        }

        void omit_error(auto&& buf) const {
            appends(buf, msg, "\n",
                    file, ":", nums(loc.line), ":", nums(loc.col), ":\n",
                    src);
        }

        void as_json(auto&& buf) const {
            auto field = buf.object();
            field("msg", msg);
            field("file", file);
            field("loc", loc);
            field("src", src);
            field("warn", warn);
        }
    };

    struct SourceError {
        using error_buffer_type = std::string;

        std::vector<SourceEntry> errs;
        std::string to_string() {
            std::string buf;
            error(buf);
            return buf;
        }

        void error(auto&& buf) {
            for (auto& err : errs) {
                err.error(buf);
                buf.push_back('\n');
            }
        }

        void for_each_error(auto&& cb) const {
            std::string b;
            for (auto& err : errs) {
                b.clear();
                err.omit_error(b);
                cb(b, err.warn);
            }
        }

        void as_json(auto&& buf) const {
            auto field = buf.object();
            field("errs", errs);
        }
    };

    static_assert(either::internal::has_error_buffer_type<SourceError>);

    struct LocationEntry {
        std::string msg;
        lexer::Loc loc;
        bool warn = false;

        bool operator==(const LocationEntry& rhs) const {
            return msg == rhs.msg && loc == rhs.loc && warn == rhs.warn;
        }
    };

    struct LocationError {
        std::source_location src;
        std::vector<LocationEntry> locations;
        [[noreturn]] void report() {
            throw *this;
        }

        [[nodiscard]] LocationError& error(lexer::Loc loc, auto&&... msg) {
            std::string buf;
            appends(buf, msg...);
            locations.push_back(LocationEntry{std::move(buf), loc});
            return *this;
        }

        LocationError& warning(lexer::Loc loc, auto&&... msg) {
            std::string buf;
            appends(buf, msg...);
            locations.push_back(LocationEntry{std::move(buf), loc, true});
            return *this;
        }

        void unique() {
            std::sort(locations.begin(), locations.end(), [](auto& lhs, auto& rhs) {
                return lhs.loc.pos.begin < rhs.loc.pos.begin;
            });
            locations.erase(std::unique(locations.begin(), locations.end()), locations.end());
        }
    };

    [[nodiscard]] inline LocationError error(lexer::Loc loc, auto&&... msg) {
        return LocationError{}.error(loc, msg...);
    }

    [[nodiscard]] inline LocationError warning(lexer::Loc loc, auto&&... msg) {
        return LocationError{}.warning(loc, msg...);
    }

    template <class T>
    using result = expected<T, LocationError>;

    template <class... Args>
    either::unexpected<LocationError> unexpect_loc_error_impl(lexer::Loc loc, std::source_location src, std::format_string<Args...> fmt, Args&&... args) {
        return unexpect(LocationError{.src = src}.error(loc, std::format(fmt, std::forward<Args>(args)...)));
    }

#define unexpect_loc_error(loc, fmt, ...) unexpect_loc_error_impl(loc, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)

}  // namespace brgen
