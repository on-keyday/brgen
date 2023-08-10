/*license*/
#pragma once
#include <string>
#include "../lexer/token.h"
#include <code/src_location.h>

namespace brgen {
    struct StreamError {
        std::string msg;
        lexer::FileIndex file;
        utils::code::SrcLoc loc;
        std::string src;

        std::string to_string(std::string_view file) {
            std::string buf;

            appends(buf, "error: ", msg, "\n",
                    file, ":", nums(loc.line + 1), ":", nums(loc.pos + 1), ":\n",
                    src);
            return buf;
        }
    };

}  // namespace brgen