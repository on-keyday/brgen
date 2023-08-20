/*license*/
#pragma once
#include <helper/expected.h>

namespace brgen {
    using namespace utils::helper::either;
    namespace either = utils::helper::either;

    constexpr auto unexpect(auto&&... e) {
        return either::unexpected(e...);
    }

}  // namespace brgen
