/*license*/
#pragma once
#include <string>
#include "../lexer/token.h"
#include <code/src_location.h>
#include "../common/util.h"
#include "expected.h"
#include <vector>

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

        void error(auto&& buf) {
            appends(buf, warn ? "warning: " : "error: ");
            omit_error(buf);
        }

        void omit_error(auto&& buf) {
            appends(buf, msg, "\n",
                    file, ":", nums(loc.line), ":", nums(loc.col), ":\n",
                    src);
        }

        void as_json(auto&& buf) {
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

        void for_each_error(auto&& cb) {
            std::string b;
            for (auto& err : errs) {
                b.clear();
                err.omit_error(b);
                cb(b, err.warn);
            }
        }

        void as_json(auto&& buf) {
            auto field = buf.object();
            field("errs", errs);
        }
    };

    static_assert(either::internal::has_error_buffer_type<SourceError>);

    struct LocationEntry {
        std::string msg;
        lexer::Loc loc;
        bool warn = false;
    };

    struct LocationError {
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
    };

    [[nodiscard]] inline LocationError error(lexer::Loc loc, auto&&... msg) {
        return LocationError{}.error(loc, msg...);
    }

    [[nodiscard]] inline LocationError warning(lexer::Loc loc, auto&&... msg) {
        return LocationError{}.warning(loc, msg...);
    }

    template <class T>
    using result = expected<T, LocationError>;

    inline std::string src_error_to_string(SourceError&& err) {
        return err.to_string();
    }

}  // namespace brgen
