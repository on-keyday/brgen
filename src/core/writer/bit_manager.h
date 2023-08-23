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
        size_t bit_size_v = 0;
        std::string_view target;
        bool is_encode = false;

        // temporary variables
        // public for debug
        std::string_view non_aligned_bit_size_v;
        std::string_view bytes_base_v;
        std::string_view remain_bits_v;
        std::string_view bytes_v;
        std::string_view tail_bits_v;

        std::string add_offset() {
            return concat(bit_index(), "+=", bit_size());
        }

       private:
        std::string bit_index() {
            return concat(io_object, accessor, index);
        }
        std::string access_buffer() {
            return concat(io_object, accessor, buffer);
        }

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

        std::string non_aligned_bit_size_with_bit_index(std::string_view i) {
            return concat("(", "(", "8", "-", lsb3(i), ")", "&", "0x7", ")");
        }

        std::string first_non_aligned_bit_size() {
            return non_aligned_bit_size_v.size() ? concat(non_aligned_bit_size_v) : non_aligned_bit_size_with_bit_index(bit_index());
        }

        std::string mask_val_n_bit_from_lsb(std::string_view i) {
            return concat("(", "0xff", ">>", "(", i, ")", ")");
        }

        std::string mask_val_remaining_bit() {
            return mask_val_n_bit_from_lsb(concat("8", "-", first_non_aligned_bit_size()));
        }

        std::string mask_remaining_bit_with_val(std::string_view val) {
            return concat("(", val, "&", mask_val_remaining_bit(), ")");
        }

        std::string current_byte_remaining_bit_masked() {
            return mask_remaining_bit_with_val(current_byte());
        }

        std::string cast_to_type(std::string_view ty, std::string_view val) {
            return concat(ty, "(", val, ")");
        }

        std::string cast_to_base(std::string_view val) {
            return cast_to_type(base_type, val);
        }

        std::string cast_to_byte(std::string_view val) {
            return cast_to_type(byte_type, val);
        }

        std::string shift_right(std::string_view val, std::string_view shift) {
            return concat(cast_to_base(val), ">>", "(", shift, ")");
        }

        std::string shift_left(std::string_view val, std::string_view shift) {
            return concat(cast_to_base(val), "<<", "(", shift, ")");
        }

        std::string shift_right_to_decode(std::string_view val, std::string_view shift) {
            return cast_to_base(shift_right(val, shift));
        }

        std::string shift_left_to_decode(std::string_view val, std::string_view shift) {
            return cast_to_base(shift_left(val, shift));
        }

        std::string shift_left_to_encode(std::string_view val, std::string_view shift) {
            return cast_to_byte(shift_left(val, shift));
        }

        std::string shift_right_to_encode(std::string_view val, std::string_view shift) {
            return cast_to_byte(shift_right(val, shift));
        }

        std::string assign(std::string_view target, std::string_view val) {
            return concat(target, " = ", val);
        }

        std::string or_assign(std::string_view target, std::string_view val) {
            return concat(target, " |= ", val);
        }

        std::string shift_count_small(std::string_view n_bit, std::string_view remain) {
            return concat("(", "(", remain, ")", "-", n_bit, ")");
        }

        std::string shift_count_large(std::string_view n_bit, std::string_view remain) {
            return concat("(", n_bit, "-", "(", remain, ")", ")");
        }

        std::string remain_bits() {
            return remain_bits_v.size() ? concat(remain_bits_v) : concat("(", bit_size(), "-", first_non_aligned_bit_size(), ")");
        }

        std::string tail_bits_shift() {
            return concat("(", "8", "-", "(", tail_bits_v, ")", ")");
        }

        std::string bytes_base() {
            return bytes_base_v.size() ? concat(bytes_base_v) : bit_to_byte(concat(bit_index(), "+", first_non_aligned_bit_size()));
        }

        std::string x_exists(std::string_view x) {
            return concat("(", x, ")", "!=", "0");
        }

       public:
        std::string enough_bits_for_bit_size_value() {
            return concat(bit_size(), "<=", first_non_aligned_bit_size());
        }

        std::string non_aligned_bits_exists() {
            return x_exists(first_non_aligned_bit_size());
        }

        std::string tail_bits_exists() {
            return x_exists(tail_bits_v);
        }

        std::string bit_size() {
            return nums(bit_size_v);
        }

        std::string define_begin_bits(std::string_view varname) {
            auto res = concat(varname, " = ", first_non_aligned_bit_size());
            non_aligned_bit_size_v = varname;
            return res;
        }

        std::string define_remain_bits(std::string_view varname) {
            auto res = concat(varname, " = ", remain_bits());
            remain_bits_v = varname;
            return res;
        }

        std::string define_bytes_base(std::string_view varname) {
            auto res = concat(varname, " = ", bytes_base());
            bytes_base_v = varname;
            return res;
        }

        std::string define_bytes(std::string_view varname) {
            auto res = concat(varname, " = ", bit_to_byte(remain_bits()));
            bytes_v = varname;
            return res;
        }

        std::string define_end_bits(std::string_view varname) {
            auto res = concat(varname, " = ", lsb3(remain_bits()));
            tail_bits_v = varname;
            return res;
        }

       private:
        std::string encode_less_than_8_bit() {
            return or_assign(current_byte(),
                             shift_left_to_encode(
                                 target,
                                 shift_count_small(
                                     bit_size(), first_non_aligned_bit_size())));
        }

        std::string decode_less_than_8_bit() {
            return or_assign(
                target,
                shift_right_to_decode(
                    current_byte_remaining_bit_masked(),
                    shift_count_small(
                        bit_size(), first_non_aligned_bit_size())));
        }

        std::string encode_begin_bits() {
            return or_assign(current_byte(),
                             shift_right_to_encode(
                                 target,
                                 shift_count_large(bit_size(), first_non_aligned_bit_size())));
        }

        std::string decode_begin_bits() {
            return or_assign(target,
                             shift_left_to_decode(
                                 current_byte_remaining_bit_masked(),
                                 shift_count_large(bit_size(), first_non_aligned_bit_size())));
        }

        std::string encode_bytes(std::string_view index) {
            return assign(byte_at(concat(bytes_base(), " + ", index)),
                          shift_right_to_encode(
                              target,
                              shift_count_large(
                                  bit_size(),
                                  concat(byte_to_bit(concat(index, "+ 1")), "+", first_non_aligned_bit_size()))));
        }

        std::string decode_bytes(std::string_view index) {
            return or_assign(target,
                             shift_left_to_decode(
                                 byte_at(concat(bytes_base(), " + ", index)),
                                 shift_count_large(
                                     bit_size(),
                                     concat(byte_to_bit(concat(index, "+ 1")), "+", first_non_aligned_bit_size()))));
        }

        std::string encode_end_bits() {
            return assign(
                byte_at(bit_to_byte(concat(bit_index(), " + ", first_non_aligned_bit_size(), "+ ", byte_to_bit(bytes_v)))),
                shift_left_to_encode(target, tail_bits_shift()));
        }

        std::string decode_end_bits() {
            return or_assign(
                target,
                shift_right_to_decode(
                    byte_at(
                        bit_to_byte(concat(bit_index(), " + ", first_non_aligned_bit_size(), "+ ", byte_to_bit(bytes_v)))),
                    tail_bits_shift()));
        }

        std::string encode_end_1_bit() {
            return assign(
                byte_at(bit_to_byte(bit_index())),
                shift_left_to_encode(target, "7"));
        }

        std::string decode_end_1_bit() {
            return or_assign(
                target,
                shift_right_to_decode(
                    byte_at(
                        bit_to_byte(concat(bit_index()))),
                    "7"));
        }

       public:
        std::string code_less_than_8_bit() {
            return is_encode ? encode_less_than_8_bit() : decode_less_than_8_bit();
        }

        std::string code_end_1_bit() {
            return is_encode ? encode_end_1_bit() : decode_end_1_bit();
        }

        std::string code_begin_bits() {
            return is_encode ? encode_begin_bits() : decode_begin_bits();
        }

        std::string code_bytes(std::string_view index) {
            return is_encode ? encode_bytes(index) : decode_bytes(index);
        }

        std::string code_end_bits() {
            return is_encode ? encode_end_bits() : decode_end_bits();
        }
    };

}  // namespace brgen::writer
