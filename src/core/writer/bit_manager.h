/*license*/
#pragma once
#include <cstdint>
#include <core/common/stack.h>
#include <vector>

namespace brgen::writer {

    struct Index {
        size_t bit_index = 0;
        size_t dynamic_ref = 0;

        constexpr size_t byte_index() const {
            return bit_index >> 3;
        }
    };

    using IndexRange = std::pair<Index, Index>;

    struct Indexer {
       private:
        Index current;
        size_t dynamic = 0;

        Stack<std::vector<IndexRange>> indexes;

       public:
        void add_bits(size_t n_bit) {
            auto begin = current;
            current.bit_index += n_bit;
            auto end = current;
            indexes.current_frame()->current.push_back({begin, end});
        }

        auto ref_scope() {
            auto old = current.dynamic_ref;
            current.dynamic_ref = dynamic + 1;
            dynamic++;
            return utils::helper::defer([=, this] {
                current.dynamic_ref = old;
            });
        }
    };

}  // namespace brgen::writer
