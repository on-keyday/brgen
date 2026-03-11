/*license*/
#pragma once
#include <functional>
#include "../converter.hpp"
#include "ebm/extended_binary_module.hpp"

namespace ebmgen {
    expected<ebm::StatementRef> xassign(ConverterContext& ctx, ebm::BinaryOp op, ebm::TypeRef type, ebm::ExpressionRef target, ebm::ExpressionRef value);

    struct BitManipulator {
       public:
        BitManipulator(ConverterContext& ctx, ebm::ExpressionRef buffer, ebm::TypeRef u8_type)
            : ctx(ctx), tmp_buffer_(buffer), u8_type(u8_type) {}

        // explain with example
        // case 1. less than 8 big endian
        //   current_bit_offset = 4
        //   target_type = uint2
        //   bit_size = 2
        //   endian = big
        //   offset = 0
        //   bit_offset = 4
        //   bit_to_read = min(8 - 4,2 - 0) = min(4,2) = 2
        //   shift = 8 - bit_to_read - bit_offset = 8 - 2 - 4 = 2
        //   mask = (1 << bit_to_read) - 1) >> shift = ((1 << 2) - 1) << 2 = 0x03 << 2 = 0x0C
        //   lsb = (tmp_buffer[offset] & mask) >> shift = (tmp_buffer[offset] & 0x0C) >> 2
        //   final_shift = bit_size - bit_processed - bit_to_read = 2 - 0 - 2 = 0
        //   result = uint2(lsb) << final_shift == uint2(lsb)
        // case 2. less than 8 little endian
        //   current_bit_offset = 4
        //   target_type = uint2
        //   bit_size = 2
        //   endian = big
        //   offset = 0
        //   bit_offset = 4
        //   bit_to_read = min(8 - 4,2 - 0) = min(4,2) = 2
        //   shift = bit_offset = 4
        //   mask = (1 << bit_to_read) - 1) << shift = ((1 << 2) - 1) << 4 = 0x03 << 4 = 0x30
        //   lsb = (tmp_buffer[offset] & mask) >> shift = (tmp_buffer[0] & 0x30) >> 4
        //   final_shift = bit_size - bit_processed - bit_to_read = 2 - 0 - 2 = 0
        //   result = uint2(lsb) << final_shift == uint2(lsb)
        // case 3. over bytes big endian
        //   current_bit_offset = 7
        //   target_type = uint10
        //   bit_size = 10
        //   endian = big
        //   offset = 0
        //   bit_offset = 7
        // for bit_processed < bit_size == 0 < 10
        //   bit_to_read = min(8 - bit_offset,bit_size - bit_processed) = min(8 - 7,10 - 0) = min(1,10) = 1
        //   shift = 8 - bit_to_read - bit_offset = 8 - 1 - 7 = 0
        //   mask = (1 << bit_to_read) - 1) << shift = ((1 << 1) - 1) << 0 = 0x01 << 0 = 0x01
        //   lsb = (tmp_buffer[offset] & mask) >> shift = (tmp_buffer[0] & 0x01) >> 0 = (tmp_buffer[0] & 0x01)
        //   final_shift = bit_size - bit_processed - bit_to_read = 10 - 0 - 1 = 9
        //   current_expr = result = uint10(lsb) << 9 = (uint10(tmp_buffer[0] & 0x01) << 9)
        //   bit_processed += 1
        //   offset++
        //   bit_offset = 0
        // for bit_processed < bit_size == 1 < 10
        //   bit_to_read = min(8 - bit_offset,bit_size - bit_processed) = min(8 - 0,10 - 1) = min(8,9) = 8
        //   shift = 8 - bit_to_read - bit_offset = 8 - 8 - 0 = 0
        //   mask = ((1 << bit_to_read) - 1) << shift = ((1 << 8) - 1) << 0 = 0xFF << 0 = 0xFF
        //   lsb = (tmp_buffer[offset] & mask) >> shift = (tmp_buffer[1] & 0xFF) >> 0 = (tmp_buffer[1] & 0xFF) = tmp_buffer[1]
        //   final_shift = bit_size - bit_processed - bit_to_read = 10 - 1 - 8 = 1
        //   result = uint10(lsb) << 1 = (uint10(tmp_buffer[1]) << 1)
        //   current_expr = current_expr | result = (uint10(tmp_buffer[0] & 0x01) << 9) | (uint10(tmp_buffer[1]) << 1)
        //   bit_processed += 8
        //   offset++
        //   bit_offset = 0
        // for bit_processed < bit_size == 9 < 10
        //   bit_to_read = min(8 - bit_offset,bit_size - bit_processed) = min(8 - 0,10 - 9) = min(8,1) = 1
        //   shift = 8 - bit_to_read - bit_offset = 8 - 1 - 0 = 7
        //   mask = ((1 << bit_to_read) - 1) << shift = ((1 << 1) - 1) << 0 = 0x1 << 7 = 0x80
        //   lsb = (tmp_buffer[offset] & mask) >> shift = (tmp_buffer[2] & 0x80) >> 7
        //   final_shift = bit_size - bit_processed - bit_to_read = 10 - 9 - 1 = 0
        //   result = uint10(lsb) << 0 = (uint10((tmp_buffer[2] & 0x80) >> 7) << 0) = uint10((tmp_buffer[2] & 0x80) >> 7)
        //   current_expr = current_expr | result = (uint10(tmp_buffer[0] & 0x01) << 9) | (uint10(tmp_buffer[1]) << 1) | uint10((tmp_buffer[2] & 0x80) >> 7)
        expected<ebm::StatementRef> read_bits(
            size_t current_bit_offset, size_t bit_size, ebm::Endian endian, ebm::TypeRef target_type, ebm::ExpressionRef dst_expr);

        expected<ebm::StatementRef> write_bits(
            size_t current_bit_offset, size_t bit_size, ebm::Endian endian, ebm::TypeRef target_type, ebm::ExpressionRef src_expr);
        expected<ebm::StatementRef> process_bits_dynamic(
            bool is_encode, ebm::ExpressionRef current_bit_offset, ebm::ExpressionRef bit_size, ebm::TypeRef target_type, std::function<expected<ebm::StatementRef>(ebm::ExpressionRef, ebm::ExpressionRef, ebm::ExpressionRef, ebm::ExpressionRef, ebm::ExpressionRef)> process);
        expected<ebm::StatementRef> read_bits_dynamic(
            ebm::ExpressionRef current_bit_offset, ebm::ExpressionRef bit_size, ebm::Endian endian, ebm::TypeRef target_type, ebm::ExpressionRef dst_expr);

        expected<ebm::StatementRef> write_bits_dynamic(
            ebm::ExpressionRef current_bit_offset, ebm::ExpressionRef bit_size, ebm::Endian endian, ebm::TypeRef target_type, ebm::ExpressionRef source_expr);
        // 値から、エンディアンを考慮して8ビットに収まる範囲を取り出す
        // expr_shift = big ? bit_size - bits_processed - bit_to_read : bits_processed
        // byte_shift = big ? (8 - bit_to_read - bit_offset) : bit_offset;
        // lsb_mask = (1 << bit_to_read) - 1
        // optimize_shift = expr_shift >= byte_shift
        // shift = optimize_shift && should_optimize ? expr_shift - byte_shift : expr_shift
        // mask = optimize_shift ? lsb_mask << byte_shift : lsb_mask
        // cast_u8 ? uint8((source_expr >> shift) & mask) : (source_expr >> shift) & mask
        expected<std::pair<ebm::ExpressionRef, bool>> extractBitsFromExpression(
            ebm::ExpressionRef source_expr,
            size_t bits_processed, size_t bit_size, size_t bit_offset, size_t bit_to_read,
            ebm::Endian endian, ebm::TypeRef target_type, bool should_optimize = true, bool cast_u8 = true);

        // 抽出したビット列を、エンディアンを考慮して最終的な値に結合するコードを生成
        // shift = big ? bit_size - bits_processed - bit_to_read : bits_processed
        // current_expr | (extracted << shift)
        expected<ebm::ExpressionRef> appendToExpression(
            std::optional<ebm::ExpressionRef> current_expr, ebm::ExpressionRef extracted,
            size_t bits_processed, size_t bit_size, size_t bit_to_read,
            ebm::Endian endian, ebm::TypeRef target_type,ebm::TypeRef src_type = ebm::TypeRef{});

       private:
        // 1バイトから特定のビット範囲を抽出し、LSBにアラインされた値にするコードを生成
        // shift = big ? (8 - bit_to_read - bit_offset) : bit_offset;
        // mask = ((1 << bit_to_read) - 1) << shift
        // (tmp_buffer[offset] & mask) >> shift
        expected<ebm::ExpressionRef> extractBitsFromByteDynamic(ebm::ExpressionRef offset, ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_to_read, ebm::Endian endian);
        // 抽出したビット列を、エンディアンを考慮して最終的な値に結合するコードを生成
        // shift = big ? bit_size - bits_processed - bit_to_read : bits_processed
        // (target_type(extracted) << shift)
        expected<ebm::ExpressionRef> appendToExpressionDynamic(ebm::ExpressionRef extracted,
                                                               ebm::ExpressionRef bits_processed, ebm::ExpressionRef bit_size, ebm::ExpressionRef bit_to_read,
                                                               ebm::Endian endian, ebm::TypeRef target_type);

        // 値から、エンディアンを考慮して8ビットに収まる範囲を取り出す
        // expr_shift = big ? bit_size - bits_processed - bit_to_read : bits_processed
        // lsb_mask = (1 << bit_to_read) - 1
        // shift =  expr_shift
        // mask = lsb_mask
        // uint8((source_expr >> shift) & mask)
        expected<ebm::ExpressionRef> extractBitsFromExpressionDynamic(
            ebm::ExpressionRef source_expr,
            ebm::ExpressionRef bits_processed, ebm::ExpressionRef bit_size, ebm::ExpressionRef bit_to_read,
            ebm::Endian endian, ebm::TypeRef target_type);

        // 1バイトに抽出されたビットを正しい位置に追加する
        // byte_shift = big ? (8 - bit_to_read - bit_offset) : bit_offset
        // shift = byte_shift
        // tmp_buffer[offset] = tmp_buffer[offset] | (extracted << shift)
        expected<ebm::ExpressionRef>
        appendBitsToByteDynamic(ebm::ExpressionRef offset, ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_to_read, ebm::Endian endian,
                                ebm::ExpressionRef extracted);

        expected<ebm::ExpressionRef> get_bit_to_read(ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_size, ebm::ExpressionRef bits_processed);

        expected<ebm::ExpressionRef> get_expr_shift_expr(ebm::Endian endian, ebm::ExpressionRef bit_size, ebm::ExpressionRef bits_processed, ebm::ExpressionRef bit_to_read) const;

        expected<ebm::ExpressionRef> get_byte_shift_expr(ebm::Endian endian, ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_to_read) const;

        expected<ebm::ExpressionRef> mask_value_expr(ebm::ExpressionRef n_bit);
        size_t get_expr_shift(ebm::Endian endian, size_t bit_size, size_t bits_processed, size_t bit_to_read) const;

        size_t get_byte_shift(ebm::Endian endian, size_t bit_offset, size_t bit_to_read) const;

        size_t mask_value(size_t n_bit) const;

        // 1バイトから特定のビット範囲を抽出し、LSBにアラインされた値にするコードを生成
        // shift = big ? (8 - bit_to_read - bit_offset) : bit_offset;
        // mask = ((1 << bit_to_read) - 1) << shift
        // (tmp_buffer[offset] & mask) >> shift
        expected<ebm::ExpressionRef> extractBitsFromByte(size_t byte_offset, size_t bit_offset, size_t bit_to_read, ebm::Endian endian);

        // 1バイトに抽出されたビットを正しい位置に追加する
        // byte_shift = big ? (8 - bit_to_read - bit_offset) : bit_offset
        // shift = optimized_shift ? 0 : byte_shift
        // mask = ((1 << bit_to_read) - 1)
        // tmp_buffer[offset] = tmp_buffer[offset] | (extracted << shift)
        expected<ebm::StatementRef> appendBitsToByte(size_t byte_offset, size_t bit_offset, size_t bit_to_read, ebm::Endian endian,
                                                     ebm::ExpressionRef extracted, bool optimized_shift);
        ebm::TypeRef u8_type;
        ConverterContext& ctx;
        ebm::ExpressionRef tmp_buffer_;
    };
}  // namespace ebmgen