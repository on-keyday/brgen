/*license*/
#pragma once
#include <helper/expected.h>

namespace brgen {
    using namespace futils::helper::either;
    namespace either = futils::helper::either;

    constexpr auto unexpect(auto&&... e) {
        return either::unexpected(e...);
    }

}  // namespace brgen
