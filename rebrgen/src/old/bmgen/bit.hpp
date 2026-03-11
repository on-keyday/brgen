/*license*/
#pragma once
#include <cstdint>
#include <binary/flags.h>

namespace rebgn {
    /*
    enum class BitDirection : std::uint8_t {
        MSB,
        LSB
    };

    struct Bit {
       private:
        futils::binary::flags_t<std::uint8_t, 3, 3, 1, 1> flags;
        bits_flag_alias_method(flags, 0, offset_from_lsb);
        bits_flag_alias_method(flags, 1, bit_count);
        bits_flag_alias_method_with_enum(flags, 2, direction, BitDirection);
        bits_flag_alias_method(flags, 3, enabled);

       public:
        constexpr Bit() noexcept
            : flags(0) {}

        constexpr explicit operator bool() const noexcept {
            return enabled();
        }

        explicit constexpr Bit(std::uint8_t n, BitDirection dir = BitDirection::MSB) noexcept {
            if (n == 0) {  // 0 is not a valid bit
                enabled(false);
                return;
            }
            for (auto i = 0; i < 8; i++) {
                if (n & (1 << i)) {
                    offset_from_lsb(i);
                    break;
                }
            }
            for (auto i = offset_from_lsb(); i < 8; i++) {
                if ((n & (1 << i)) == 0) {
                    bit_count(i - offset_from_lsb());
                    break;
                }
            }
            if (bit_count() == 0) {
                bit_count(7 - offset_from_lsb());
            }
            for (auto i = offset_from_lsb() + bit_count() + 1; i < 8; i++) {
                if (n & (1 << i)) {
                    bit_count(0);
                    offset_from_lsb(0);
                    enabled(false);
                    return;
                }
            }
            direction(dir);
            enabled(true);
        }

        constexpr BitDirection get_direction() const noexcept {
            return direction();
        }

        constexpr std::uint8_t get_offset_from_lsb() const noexcept {
            return offset_from_lsb();
        }

        constexpr std::uint8_t get_offset_from_msb() const noexcept {
            return 7 - offset_from_lsb();
        }

        constexpr std::uint8_t get_bit_count() const noexcept {
            return bit_count() + 1;
        }

        constexpr std::uint8_t bit_mask() const noexcept {
            auto mask = (1 << get_bit_count()) - 1;
            auto shift = offset_from_lsb();
            return mask << shift;
        }
    };

    constexpr auto sizeof_Bit = sizeof(Bit);
    constexpr auto lsbBit = Bit(0b00000001);
    constexpr auto secondLsbBit = Bit(0b00000010);
    constexpr auto thirdLsbBit = Bit(0b00000100);
    constexpr auto fourthLsbBit = Bit(0b00001000);
    constexpr auto fifthLsbBit = Bit(0b00010000);
    constexpr auto sixthLsbBit = Bit(0b00100000);
    constexpr auto seventhLsbBit = Bit(0b01000000);
    constexpr auto msbBit = Bit(0b10000000);

    namespace test {
        static_assert(!Bit(0b101010101));
    }  // namespace test

    struct Byte {
       private:
        Bit bits[8];

       public:
        constexpr Byte() noexcept = default;

        constexpr explicit Byte(std::uint8_t n) noexcept {
            for (auto i = 0; i < 8; i++) {
                bits[i] = Bit(n & (1 << i) ? 1 << i : 0);
            }
        }

        constexpr void set(Bit b) noexcept {
            if (!b) {
                return;
            }
            if (overlap(b)) {
                return;
            }
            bits[b.get_offset_from_lsb()] = b;
            for (size_t i = 1; i < b.get_bit_count(); i++) {
                bits[b.get_offset_from_lsb() + i] = Bit();
            }
        }

        constexpr void unset(Bit b) noexcept {
            if (!b) {
                return;
            }
            if (!overlap(b)) {
                return;
            }
            if (bits[b.get_offset_from_lsb()].bit_mask() != b.bit_mask()) {
                return;
            }
            bits[b.get_offset_from_lsb()] = Bit();
        }

        constexpr void clear() noexcept {
            for (auto& b : bits) {
                b = Bit();
            }
        }

        constexpr bool overlap(const Bit& b) const noexcept {
            for (auto& bit : bits) {
                if (bit && (bit.bit_mask() & b.bit_mask())) {
                    return true;
                }
            }
            return false;
        }
    };

    constexpr auto sizeof_Byte = sizeof(Byte);

    template <size_t N>
    struct Uint {
       private:
        Byte bytes[N];

       public:
        constexpr Uint() noexcept = default;

        constexpr explicit Uint(std::uint64_t n) noexcept {
            for (auto i = 0; i < N; i++) {
                bytes[i] = Byte(n & 0xFF);
                n >>= 8;
            }
        }
    };
    */

    constexpr std::uint64_t safe_left_shift(std::uint64_t x, std::uint64_t y) {
        if (y >= 64) {
            return 0;
        }
        return x << y;
    }
}  // namespace rebgn
