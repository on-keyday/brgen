/*license*/
#pragma once
#include <string>
#include "../lexer/token.h"
#include <code/src_location.h>
#include "../common/util.h"

namespace brgen {
    struct SourceError {
        std::string msg;
        std::string file;
        utils::code::SrcLoc loc;
        std::string src;

        std::string to_string() {
            std::string buf;
            appends(buf, "error: ", msg, "\n",
                    file, ":", nums(loc.line + 1), ":", nums(loc.pos + 1), ":\n",
                    src);
            return buf;
        }
    };

    struct LocationEntry {
        std::string msg;
        lexer::Loc loc;
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
    };

    [[nodiscard]] inline LocationError error(lexer::Loc loc, auto&&... msg) {
        return LocationError{}.error(loc, msg...);
    }

}  // namespace brgen
