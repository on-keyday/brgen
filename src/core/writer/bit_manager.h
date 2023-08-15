/*license*/
#pragma once
#include <cstdint>
#include <core/common/stack.h>
#include <vector>

namespace brgen::writer {

    struct Index {
        size_t byte_index = 0;
        std::uint8_t bit_index = 0;
        size_t dynamic_ref = 0;
    };

    struct Indexer {
       private:
        Index current;
        size_t dynamic = 0;

        Stack<std::vector<Index>> indexes;

       public:
    };

}  // namespace brgen::writer
