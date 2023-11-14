
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

    struct BitFlags {
        BuiltinType base_type;
        std::vector<std::uint8_t> bits;
    };

    struct Bytes {};

    struct Field {
        std::vector<std::string> name;
    };

}  // namespace j2cp2
