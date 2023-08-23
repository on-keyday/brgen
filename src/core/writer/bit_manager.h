/*license*/
#pragma once
#include <cstdint>
#include <core/common/stack.h>
#include <vector>
#include <core/common/util.h>

namespace brgen::writer {

    struct IOCodeManager {
        std::string_view io_object;
        std::string_view accessor;
        std::string_view index;
        std::string_view buffer;
        std::string_view byte_type;

        std::string access_index() {
            return concat(io_object, accessor, index);
        }

        std::string access_buffer() {
            return concat(io_object, accessor, buffer);
        }

        std::string byte_with_index(std::string_view i) {
            return concat(access_buffer(), "[", i, "]");
        }

        std::string byte() {
            return byte_with_index(byte_index());
        }

        std::string byte_index_with_bit_index(std::string_view i) {
            return concat("(", i, ">>", "3", ")");
        }

        std::string byte_index() {
            return byte_index_with_bit_index(access_index());
        }

        std::string bit_pos_with_bit_index(std::string_view i) {
            return concat("(", i, "&", "0x7", ")");
        }

        std::string bit_pos() {
            return bit_pos_with_bit_index(access_index());
        }

        std::string remaining_bit_size_with_bit_index(std::string_view i) {
            return concat("(", "(", "8", "-", bit_pos_with_bit_index(i), ")", "&", "0x7", ")");
        }

        std::string remaining_bit_size() {
            return remaining_bit_size_with_bit_index(access_index());
        }

        std::string mask_val_n_bit_from_lsb(std::string_view i) {
            return concat("(", "0xff", ">>", i, ")");
        }

        std::string mask_val_remaining_bit() {
            return mask_val_n_bit_from_lsb(remaining_bit_size());
        }

        std::string mask_remaining_bit_with_val(std::string_view val) {
            return concat("(", val, "&", mask_val_remaining_bit(), ")");
        }

        std::string mask_remaining_bit() {
            return mask_remaining_bit_with_val(byte());
        }

        std::string shift_to_decode_small(std::string_view base_ty, std::string_view val, std::string_view shift) {
            return concat("(", base_ty, "(", val, ")", ">>", shift, ")");
        }

        std::string shift_to_decode_large(std::string_view base_ty, std::string_view val, std::string_view shift) {
            return concat("(", base_ty, "(", val, ")", "<<", shift, ")");
        }

        std::string shift_to_encode_small(std::string_view val, std::string_view shift) {
            return concat("(", byte_type, "(", val, ")", "<<", shift, ")");
        }

        std::string shift_to_encode_large(std::string_view val, std::string_view shift) {
            return concat("(", byte_type, "(", val, ")", ">>", shift, ")");
        }

        std::string assign(std::string_view target, std::string_view val) {
            return concat(target, " = ", val);
        }

        std::string or_assign(std::string_view target, std::string_view val) {
            return concat(target, " |= ", val);
        }
    };

}  // namespace brgen::writer
