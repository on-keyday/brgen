
#include <cstdint>
#include <vector>
#include <string>

namespace j2cp2 {
    enum class BuiltinType {
        uint8,
        uint16,
        uint32,
        uint64,
        uint128,
    };

    struct BitField {
        std::string name;
        std::uint8_t bit = 0;
    };

    struct BitFlags {
        BuiltinType type;
        std::vector<BitField> bit;
    };

    struct Bytes {};
}  // namespace j2cp2
