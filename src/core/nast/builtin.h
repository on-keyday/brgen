/*license*/
#include "nodes.h"
#include <number/parse.h>

namespace brgen::nast {

    struct IntTypeDesc {
        size_t bit_size = 0;
        Endian endian = Endian::unspec;
        bool is_signed = false;
    };

    constexpr auto float_prefix = 'f';
    constexpr auto big_endian_prefix = 'b';
    constexpr auto little_endian_prefix = 'l';
    constexpr auto signed_prefix = 'i';
    constexpr auto unsigned_prefix = 'u';

    inline Endian detect_endian_prefix(std::string_view str) {
        if (str.empty()) {
            return Endian::unspec;
        }
        if (str[0] == big_endian_prefix) {
            return Endian::big;
        }
        else if (str[0] == little_endian_prefix) {
            return Endian::little;
        }
        return Endian::unspec;
    }

    inline std::optional<IntTypeDesc> is_int_type(std::string_view str) {
        if (str.empty()) {
            return std::nullopt;
        }
        IntTypeDesc desc;
        if (str[0] == signed_prefix) {
            desc.is_signed = true;
            str = str.substr(1);
        }
        else if (str[0] == unsigned_prefix) {
            desc.is_signed = false;
            str = str.substr(1);
        }
        else {
            return std::nullopt;
        }
        if (str.empty()) {
            return std::nullopt;
        }
        desc.endian = detect_endian_prefix(str);
        if (desc.endian != Endian::unspec) {
            str = str.substr(1);
        }
        // Check if the string starts with 'u' or 'b' and has a valid unsigned integer
        size_t value = 0;
        if (!futils::number::parse_integer(str, value)) {
            return std::nullopt;
        }
        if (value == 0) {  // u0 is not valid
            return std::nullopt;
        }
        desc.bit_size = value;
        return desc;
    }

    struct FloatTypeDesc {
        size_t bit_size = 0;
        Endian endian = Endian::unspec;
    };

    inline std::optional<FloatTypeDesc> is_float_type(std::string_view str) {
        if (str.empty()) {
            return std::nullopt;
        }
        FloatTypeDesc desc;
        if (str[0] != float_prefix) {
            return std::nullopt;
        }
        str = str.substr(1);
        if (str.empty()) {
            return std::nullopt;
        }
        desc.endian = detect_endian_prefix(str);
        if (desc.endian != Endian::unspec) {
            str = str.substr(1);
        }
        // Check if the string starts with 'u' or 'b' and has a valid unsigned integer
        size_t value = 0;
        if (!futils::number::parse_integer(str, value)) {
            return std::nullopt;
        }
        if (value == 0) {  // u0 is not valid
            return std::nullopt;
        }
        desc.bit_size = value;
        return desc;
    }
}  // namespace brgen::nast