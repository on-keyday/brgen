/*license*/
#pragma once
#include <cstdint>
#include <core/common/stack.h>
#include <vector>
#include <core/common/util.h>

namespace brgen::writer {

    struct BitIOCodeGenerator {
        // configs
        std::string_view io_object;
        std::string_view accessor;
        std::string_view index;
        std::string_view buffer;
        std::string_view byte_type;
        std::string_view base_type;
        size_t bit_size = 0;

        // cache
        std::string_view non_aligned_bit_size_v;
        std::string_view bytes_base_v;
        std::string_view bytes_v;
        std::string_view tail_bits_v;

        std::string bit_index() {
            return concat(io_object, accessor, index);
        }

       private:
        std::string access_buffer() {
            return concat(io_object, accessor, buffer);
        }

       public:
        std::string byte_at(std::string_view i) {
            return concat(access_buffer(), "[", i, "]");
        }

        std::string current_byte() {
            return byte_at(byte_index());
        }

        std::string bit_to_byte(std::string_view bit) {
            return concat("(", "(", bit, ")", ">>", "3", ")");
        }

        std::string byte_to_bit(std::string_view byt) {
            return concat("(", "(", byt, ")", "<<", "3", ")");
        }

        std::string byte_index() {
            return bit_to_byte(bit_index());
        }

        std::string lsb3(std::string_view byt) {
            return concat("(", byt, "&", "0x7", ")");
        }

       private:
        std::string non_aligned_bit_size_with_bit_index(std::string_view i) {
            return concat("(", "(", "8", "-", lsb3(i), ")", "&", "0x7", ")");
        }

       public:
        std::string non_aligned_bit_size() {
            return non_aligned_bit_size_v.size() ? concat(non_aligned_bit_size_v) : non_aligned_bit_size_with_bit_index(bit_index());
        }

       private:
        std::string mask_val_n_bit_from_lsb(std::string_view i) {
            return concat("(", "0xff", ">>", "(", i, ")", ")");
        }

        std::string mask_val_remaining_bit() {
            return mask_val_n_bit_from_lsb(concat("8", "-", non_aligned_bit_size()));
        }

        std::string mask_remaining_bit_with_val(std::string_view val) {
            return concat("(", val, "&", mask_val_remaining_bit(), ")");
        }

       public:
        std::string byte_remaining_bit_masked() {
            return mask_remaining_bit_with_val(current_byte());
        }

       private:
        std::string cast_to_type(std::string_view ty, std::string_view val) {
            return concat(ty, "(", val, ")");
        }

       public:
        std::string cast_to_base(std::string_view val) {
            return cast_to_type(base_type, val);
        }

        std::string cast_to_byte(std::string_view val) {
            return cast_to_type(byte_type, val);
        }

        std::string shift_right_to_decode(std::string_view val, std::string_view shift) {
            return cast_to_base(concat(cast_to_base(val), ">>", "(", shift, ")"));
        }

        std::string shift_left_to_decode(std::string_view val, std::string_view shift) {
            return cast_to_base(concat(cast_to_base(val), "<<", "(", shift, ")"));
        }

        std::string shift_left_to_encode(std::string_view val, std::string_view shift) {
            return cast_to_byte(concat(cast_to_base(val), "<<", "(", shift, ")"));
        }

        std::string shift_right_to_encode(std::string_view val, std::string_view shift) {
            return cast_to_byte(concat(cast_to_base(val), ">>", "(", shift, ")"));
        }

        std::string assign(std::string_view target, std::string_view val) {
            return concat(target, " = ", val);
        }

        std::string or_assign(std::string_view target, std::string_view val) {
            return concat(target, " |= ", val);
        }

        std::string enough_bits_for_bit_size_value() {
            return concat(nums(bit_size), "<=", non_aligned_bit_size());
        }

        std::string shift_count_small(std::string_view n_bit, std::string_view remain) {
            return concat("(", "(", remain, ")", "-", n_bit, ")");
        }

        std::string shift_count_large(std::string_view n_bit, std::string_view remain) {
            return concat("(", n_bit, "-", "(", remain, ")", ")");
        }

        std::string remain_bits_large(std::string_view n_bit) {
            return concat("(", n_bit, "-", non_aligned_bit_size(), ")");
        }

        std::string tail_bits_shift() {
            return concat("(", "8", "-", "(", tail_bits_v, ")", ")");
        }

        std::string bytes_base() {
            return bytes_base_v.size() ? concat(bytes_base_v) : bit_to_byte(concat(bit_index(), "+", non_aligned_bit_size()));
        }

       private:
        std::string x_exists(std::string_view x) {
            return concat("(", x, ")", "!=", "0");
        }

       public:
        std::string non_aligned_bits_exists() {
            return x_exists(non_aligned_bit_size());
        }

        std::string tail_bits_exists() {
            return x_exists(tail_bits_v);
        }
    };

}  // namespace brgen::writer
