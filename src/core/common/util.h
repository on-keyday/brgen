/*license*/
#pragma once

#include <string>

namespace brgen {
    auto nums(auto v, int radix = 10) {
        return utils::number::to_string<std::string>(v, radix);
    }

    using utils::strutil::append, utils::strutil::appends;

}  // namespace brgen
