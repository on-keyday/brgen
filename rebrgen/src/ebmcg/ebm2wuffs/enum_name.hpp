/*license*/
#pragma once
// Shared helper for ebm2wuffs enum-member name conversion. Wuffs const names
// must match [_0-9A-Z]+ (lang/parse/parse.go validConstName), but EBM enum
// members can be PascalCase or camelCase. Convert to UPPER_SNAKE_CASE so the
// `pub const NAME : ...` declaration and the EnumMember reference agree.
#include <cctype>
#include <string>
#include <string_view>

inline std::string ebm2wuffs_enum_member_to_upper_snake(std::string_view s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (i > 0) {
            unsigned char prev = static_cast<unsigned char>(s[i - 1]);
            bool boundary_low_to_up = (std::islower(prev) || std::isdigit(prev)) && std::isupper(c);
            bool boundary_acronym_to_word =
                std::isupper(prev) && std::isupper(c) && i + 1 < s.size() && std::islower(static_cast<unsigned char>(s[i + 1]));
            if (boundary_low_to_up || boundary_acronym_to_word) {
                r += '_';
            }
        }
        r += static_cast<char>(std::toupper(c));
    }
    return r;
}
