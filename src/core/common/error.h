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
        utils::code::SrcLoc loc;
        std::string src;
        bool warn = false;

        std::string to_string() {
            std::string buf;
            error(buf);
            return buf;
        }

        void error(auto&& buf) {
            appends(buf, warn ? "warning: " : "error: ", msg, "\n",
                    file, ":", nums(loc.line + 1), ":", nums(loc.pos + 1), ":\n",
                    src);
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
