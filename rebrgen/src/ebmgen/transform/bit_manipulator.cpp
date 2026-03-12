/*license*/
#include "bit_manipulator.hpp"
#include "../convert/helper.hpp"
#include "ebmgen/converter.hpp"

namespace ebmgen {
    expected<ebm::StatementRef> xassign(ConverterContext& ctx, ebm::BinaryOp op, ebm::TypeRef type, ebm::ExpressionRef target, ebm::ExpressionRef value) {
        EBM_BINARY_OP(operated, op, type, target, value);
        EBM_ASSIGNMENT(assign, target, operated);
        return assign;
    }

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
    expected<ebm::StatementRef> BitManipulator::read_bits(
        size_t current_bit_offset, size_t bit_size, ebm::Endian endian, ebm::TypeRef target_type, ebm::ExpressionRef dst_expr) {
        std::optional<ebm::ExpressionRef> combined_expr;
        size_t bits_processed = 0;
        size_t offset = current_bit_offset / 8;
        size_t bit_offset = current_bit_offset % 8;

        while (bits_processed < bit_size) {
            const size_t bits_to_read = std::min((size_t)8 - bit_offset, bit_size - bits_processed);

            MAYBE(extracted_part, extractBitsFromByte(offset, bit_offset, bits_to_read, endian));
            MAYBE(new_expr, appendToExpression(combined_expr, extracted_part, bits_processed, bit_size, bits_to_read, endian, target_type));
            combined_expr = new_expr;

            bits_processed += bits_to_read;
            offset++;
            bit_offset = 0;
        }

        if (!combined_expr) {
            return unexpect_error("Failed to generate bit extraction expression");
        }

        EBM_ASSIGNMENT(assign, dst_expr, *combined_expr);
        return assign;
    }

    expected<ebm::StatementRef> BitManipulator::write_bits(
        size_t current_bit_offset, size_t bit_size, ebm::Endian endian, ebm::TypeRef target_type, ebm::ExpressionRef src_expr) {
        std::optional<ebm::ExpressionRef> combined_expr;
        size_t bits_processed = 0;
        size_t offset = current_bit_offset / 8;
        size_t bit_offset = current_bit_offset % 8;
        ebm::Block block;

        while (bits_processed < bit_size) {
            const size_t bits_to_read = std::min((size_t)8 - bit_offset, bit_size - bits_processed);

            MAYBE(extracted_part, extractBitsFromExpression(src_expr, bits_processed, bit_size, bit_offset, bits_to_read, endian, target_type));
            MAYBE(new_assign, appendBitsToByte(offset, bit_offset, bits_to_read, endian, extracted_part.first, extracted_part.second));
            append(block, new_assign);

            bits_processed += bits_to_read;
            offset++;
            bit_offset = 0;
        }

        if (block.container.empty()) {
            return unexpect_error("Failed to generate bit extraction expression");
        }
        if (block.container.size() == 1) {
            return block.container[0];
        }

        EBM_BLOCK(assign, std::move(block));
        return assign;
    }

    expected<ebm::StatementRef> BitManipulator::process_bits_dynamic(
        bool is_encode, ebm::ExpressionRef current_bit_offset, ebm::ExpressionRef bit_size, ebm::TypeRef target_type, std::function<expected<ebm::StatementRef>(ebm::ExpressionRef, ebm::ExpressionRef, ebm::ExpressionRef, ebm::ExpressionRef, ebm::ExpressionRef)> process) {
        EBMU_COUNTER_TYPE(counter_t);
        EBMU_INT_LITERAL(eight, 8);
        EBM_BINARY_OP(offset_start, ebm::BinaryOp::div, counter_t, current_bit_offset, eight);
        EBM_BINARY_OP(bit_offset_start, ebm::BinaryOp::mod, counter_t, current_bit_offset, eight);
        EBM_DEFINE_ANONYMOUS_VARIABLE(offset, counter_t, offset_start);
        EBM_DEFINE_ANONYMOUS_VARIABLE(bit_offset, counter_t, bit_offset_start);
        EBMU_INT_LITERAL(zero, 0);
        EBM_DEFINE_ANONYMOUS_VARIABLE(bit_processed, counter_t, zero);
        EBMU_BOOL_TYPE(bool_t);
        EBM_BINARY_OP(condition, ebm::BinaryOp::less, bool_t, bit_processed, bit_size);
        MAYBE(bit_to_read_v, get_bit_to_read(bit_offset, bit_size, bit_processed));
        ebm::Block inner;
        {
            ebm::ExpressionRef is_zero;
            if (is_encode) {
                // when encoding, the target is tmp_buffer and bits are not assigned when bit_offset is zero
                EBM_BINARY_OP(is_zero_, ebm::BinaryOp::equal, bool_t, bit_offset, zero);
                is_zero = is_zero_;
            }
            else {
                // when decoding, the target is self and bits are not assigned when bit_processed is zero
                EBM_BINARY_OP(is_zero_, ebm::BinaryOp::equal, bool_t, bit_processed, zero);
                is_zero = is_zero_;
            }
            EBM_DEFINE_VARIABLE(bit_to_read, {}, counter_t, bit_to_read_v, ebm::VariableDeclKind::IMMUTABLE, false);
            MAYBE(body, process(offset, bit_offset, bit_to_read, bit_processed, is_zero));
            EBM_INCREMENT(offset_inc, offset, counter_t);
            EBM_ASSIGNMENT(bit_offset_zero, bit_offset, zero);
            MAYBE(update_bit_processed, xassign(ctx, ebm::BinaryOp::add, counter_t, bit_processed, bit_to_read));
            append(inner, bit_to_read_def);
            append(inner, body);
            append(inner, offset_inc);
            append(inner, bit_offset_zero);
            append(inner, update_bit_processed);
        }
        EBM_BLOCK(body, std::move(inner));
        EBM_WHILE_LOOP(loop_, condition, body);
        ebm::Block outer;
        append(outer, offset_def);
        append(outer, bit_offset_def);
        append(outer, bit_processed_def);
        append(outer, loop_);
        EBM_BLOCK(operation, std::move(outer));
        return operation;
    }
    expected<ebm::StatementRef> BitManipulator::read_bits_dynamic(
        ebm::ExpressionRef current_bit_offset, ebm::ExpressionRef bit_size, ebm::Endian endian, ebm::TypeRef target_type, ebm::ExpressionRef dst_expr) {
        return process_bits_dynamic(false, current_bit_offset, bit_size, target_type, [&](ebm::ExpressionRef offset, ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_to_read, ebm::ExpressionRef bit_processed, ebm::ExpressionRef is_zero) -> expected<ebm::StatementRef> {
            MAYBE(extracted, extractBitsFromByteDynamic(offset, bit_offset, bit_to_read, endian));
            MAYBE(to_append, appendToExpressionDynamic(extracted, bit_processed, bit_size, bit_to_read, endian, target_type));
            EBM_ASSIGNMENT(if_zero, dst_expr, to_append);
            MAYBE(if_non_zero, xassign(ctx, ebm::BinaryOp::bit_or, target_type, dst_expr, to_append));
            EBM_IF_STATEMENT(append, is_zero, if_zero, if_non_zero);
            return append;
        });
    }

    expected<ebm::StatementRef> BitManipulator::write_bits_dynamic(
        ebm::ExpressionRef current_bit_offset, ebm::ExpressionRef bit_size, ebm::Endian endian, ebm::TypeRef target_type, ebm::ExpressionRef source_expr) {
        return process_bits_dynamic(true, current_bit_offset, bit_size, target_type, [&](ebm::ExpressionRef offset, ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_to_read, ebm::ExpressionRef bit_processed, ebm::ExpressionRef is_zero) -> expected<ebm::StatementRef> {
            MAYBE(extracted, extractBitsFromExpressionDynamic(source_expr, bit_processed, bit_size, bit_to_read, endian, target_type));
            MAYBE(to_append, appendBitsToByteDynamic(offset, bit_offset, bit_to_read, endian, extracted));
            EBM_INDEX(byte_val, u8_type, tmp_buffer_, offset);
            EBM_ASSIGNMENT(if_zero, byte_val, to_append);
            MAYBE(if_non_zero, xassign(ctx, ebm::BinaryOp::bit_or, u8_type, byte_val, to_append));
            EBM_IF_STATEMENT(append, is_zero, if_zero, if_non_zero);
            return append;
        });
    }

    // 1バイトから特定のビット範囲を抽出し、LSBにアラインされた値にするコードを生成
    // shift = big ? (8 - bit_to_read - bit_offset) : bit_offset;
    // mask = ((1 << bit_to_read) - 1) << shift
    // (tmp_buffer[offset] & mask) >> shift
    expected<ebm::ExpressionRef> BitManipulator::extractBitsFromByteDynamic(ebm::ExpressionRef offset, ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_to_read, ebm::Endian endian) {
        EBM_INDEX(byte_val, u8_type, tmp_buffer_, offset);

        // マスクを作成: (1 << bit_to_read) - 1 で bit_to_read 個の1を作り、正しい位置へシフト
        MAYBE(shift, get_byte_shift_expr(endian, bit_offset, bit_to_read));

        MAYBE(mask, mask_value_expr(bit_to_read));
        EBM_BINARY_OP(shifted_mask, ebm::BinaryOp::left_shift, u8_type, mask, shift);
        EBM_BINARY_OP(masked, ebm::BinaryOp::bit_and, u8_type, byte_val, shifted_mask);

        // LSBにアラインするために右シフト
        EBM_BINARY_OP(extracted, ebm::BinaryOp::right_shift, u8_type, masked, shift);
        return extracted;
    }

    // 抽出したビット列を、エンディアンを考慮して最終的な値に結合するコードを生成
    // shift = big ? bit_size - bits_processed - bit_to_read : bits_processed
    // (target_type(extracted) << shift)
    expected<ebm::ExpressionRef> BitManipulator::appendToExpressionDynamic(ebm::ExpressionRef extracted,
                                                                           ebm::ExpressionRef bits_processed, ebm::ExpressionRef bit_size, ebm::ExpressionRef bit_to_read,
                                                                           ebm::Endian endian, ebm::TypeRef target_type) {
        EBM_CAST(casted_new_bits, target_type, u8_type, extracted);

        MAYBE(final_shift, get_expr_shift_expr(endian, bit_size, bits_processed, bit_to_read));

        auto result = casted_new_bits;
        EBM_BINARY_OP(shifted_bits, ebm::BinaryOp::left_shift, target_type, casted_new_bits, final_shift);

        return shifted_bits;
    }

    // 値から、エンディアンを考慮して8ビットに収まる範囲を取り出す
    // expr_shift = big ? bit_size - bits_processed - bit_to_read : bits_processed
    // lsb_mask = (1 << bit_to_read) - 1
    // shift =  expr_shift
    // mask = lsb_mask
    // uint8((source_expr >> shift) & mask)
    expected<ebm::ExpressionRef> BitManipulator::extractBitsFromExpressionDynamic(
        ebm::ExpressionRef source_expr,
        ebm::ExpressionRef bits_processed, ebm::ExpressionRef bit_size, ebm::ExpressionRef bit_to_read,
        ebm::Endian endian, ebm::TypeRef target_type) {
        MAYBE(shift, get_expr_shift_expr(endian, bit_size, bits_processed, bit_to_read));
        MAYBE(mask, mask_value_expr(bit_to_read));

        EBM_BINARY_OP(shifted_bits, ebm::BinaryOp::right_shift, target_type, source_expr, shift);

        EBM_BINARY_OP(masked, ebm::BinaryOp::bit_and, target_type, shifted_bits, mask);
        EBM_CAST(casted, u8_type, target_type, masked);

        return casted;
    }
    // 1バイトに抽出されたビットを正しい位置に追加する
    // byte_shift = big ? (8 - bit_to_read - bit_offset) : bit_offset
    // shift = byte_shift
    // tmp_buffer[offset] = tmp_buffer[offset] | (extracted << shift)
    expected<ebm::ExpressionRef> BitManipulator::appendBitsToByteDynamic(ebm::ExpressionRef offset, ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_to_read, ebm::Endian endian,
                                                                         ebm::ExpressionRef extracted) {
        MAYBE(shift, get_byte_shift_expr(endian, bit_offset, bit_to_read));
        // 元の位置におくために左シフト
        EBM_BINARY_OP(shifted, ebm::BinaryOp::left_shift, u8_type, extracted, shift);
        return shifted;
    }

    expected<ebm::ExpressionRef> BitManipulator::get_bit_to_read(ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_size, ebm::ExpressionRef bits_processed) {
        EBMU_INT_LITERAL(eight, 8);
        EBMU_BOOL_TYPE(bool_t);
        EBMU_COUNTER_TYPE(counter_t);
        // 8 - bit_offset
        EBM_BINARY_OP(eight_bit_offset, ebm::BinaryOp::sub, counter_t, eight, bit_offset);
        // bit_size - bit_processed
        EBM_BINARY_OP(size_processed, ebm::BinaryOp::sub, counter_t, bit_size, bits_processed);
        EBM_BINARY_OP(compare, ebm::BinaryOp::less, bool_t, eight_bit_offset, size_processed);

        // min(8 - bit_offset,bit_size - bit_processed)
        MAYBE(body, make_conditional(ctx, counter_t, compare, eight_bit_offset, size_processed));
        EBMA_ADD_EXPR(bit_to_read, std::move(body));
        return bit_to_read;
    }

    expected<ebm::ExpressionRef> BitManipulator::get_expr_shift_expr(ebm::Endian endian, ebm::ExpressionRef bit_size, ebm::ExpressionRef bits_processed, ebm::ExpressionRef bit_to_read) const {
        if (endian == ebm::Endian::big) {
            EBMU_COUNTER_TYPE(counter_t);
            // bit_size - bits_processed
            EBM_BINARY_OP(size_processed, ebm::BinaryOp::sub, counter_t, bit_size, bits_processed);
            // bit_size - bits_processed - bit_to_read
            EBM_BINARY_OP(shift, ebm::BinaryOp::sub, counter_t, size_processed, bit_to_read);
            // Big Endian: MSB側から詰める。後から来るビットほど下位になる。
            return shift;
        }
        else {  // Little Endian
            // Little Endian: LSB側から詰める。後から来るビットほど上位になる。
            return bits_processed;
        }
    }

    expected<ebm::ExpressionRef> BitManipulator::get_byte_shift_expr(ebm::Endian endian, ebm::ExpressionRef bit_offset, ebm::ExpressionRef bit_to_read) const {
        if (endian == ebm::Endian::big) {
            EBMU_COUNTER_TYPE(counter_t);
            EBMU_INT_LITERAL(eight, 8);
            // 8 - bit_offset
            EBM_BINARY_OP(eight_bit_offset, ebm::BinaryOp::sub, counter_t, eight, bit_offset);
            // 8 - bit_offset - bit_to_read
            EBM_BINARY_OP(shift, ebm::BinaryOp::sub, counter_t, eight_bit_offset, bit_to_read);
            // Big Endian: MSB側から詰める。後から来るビットほど下位になる。
            return shift;
        }
        else {  // Little Endian
            // Little Endian: LSB側から詰める。後から来るビットほど上位になる。
            return bit_offset;
        }
    }

    expected<ebm::ExpressionRef> BitManipulator::mask_value_expr(ebm::ExpressionRef n_bit) {
        EBMU_INT_LITERAL(one, 1);
        auto src_ty = ctx.repository().get_expression(one)->body.type;
        // for bit shift overflow safety (in most case, this is translated to wider type like uint16)
        // n_bit can be max 8, so use uint9
        EBMU_UINT_TYPE(u9_t, 9);
        EBMU_U8(u8_t);
        EBM_CAST(casted, u9_t, src_ty, one);
        EBM_BINARY_OP(shift, ebm::BinaryOp::left_shift, u9_t, one, n_bit);
        EBM_BINARY_OP(mask, ebm::BinaryOp::sub, u9_t, shift, one);
        EBM_CAST(result, u8_t, u9_t, mask);
        return result;
    }

    size_t BitManipulator::get_expr_shift(ebm::Endian endian, size_t bit_size, size_t bits_processed, size_t bit_to_read) const {
        if (endian == ebm::Endian::big) {
            // Big Endian: MSB側から詰める。後から来るビットほど下位になる。
            return bit_size - bits_processed - bit_to_read;
        }
        else {  // Little Endian
            // Little Endian: LSB側から詰める。後から来るビットほど上位になる。
            return bits_processed;
        }
    }

    size_t BitManipulator::get_byte_shift(ebm::Endian endian, size_t bit_offset, size_t bit_to_read) const {
        if (endian == ebm::Endian::big) {
            return (8 - bit_to_read - bit_offset);
        }
        else {
            return bit_offset;
        }
    }

    size_t BitManipulator::mask_value(size_t n_bit) const {
        return (1ULL << n_bit) - 1;
    }

    // 1バイトから特定のビット範囲を抽出し、LSBにアラインされた値にするコードを生成
    // shift = big ? (8 - bit_to_read - bit_offset) : bit_offset;
    // mask = ((1 << bit_to_read) - 1) << shift
    // (tmp_buffer[offset] & mask) >> shift
    expected<ebm::ExpressionRef> BitManipulator::extractBitsFromByte(size_t byte_offset, size_t bit_offset, size_t bit_to_read, ebm::Endian endian) {
        EBMU_INT_LITERAL(offset, byte_offset);
        EBM_INDEX(byte_val, u8_type, tmp_buffer_, offset);

        // マスクを作成: (1 << bit_to_read) - 1 で bit_to_read 個の1を作り、正しい位置へシフト
        const auto shift = get_byte_shift(endian, bit_offset, bit_to_read);

        assert(bit_to_read <= 8);
        const std::uint8_t mask = ((1U << bit_to_read) - 1) << shift;
        ebm::ExpressionRef result = byte_val;
        if (mask != 0xff) {
            EBMU_INT_LITERAL(mask_val, mask);
            EBM_BINARY_OP(masked, ebm::BinaryOp::bit_and, u8_type, result, mask_val);
            result = masked;
        }
        // LSBにアラインするために右シフト
        if (shift > 0) {
            EBMU_INT_LITERAL(shift_val, shift);
            EBM_BINARY_OP(shifted, ebm::BinaryOp::right_shift, u8_type, result, shift_val);
            result = shifted;
        }
        return result;
    }

    // 抽出したビット列を、エンディアンを考慮して最終的な値に結合するコードを生成
    // shift = big ? bit_size - bits_processed - bit_to_read : bits_processed
    // current_expr | (extracted << shift)
    expected<ebm::ExpressionRef> BitManipulator::appendToExpression(
        std::optional<ebm::ExpressionRef> current_expr, ebm::ExpressionRef extracted,
        size_t bits_processed, size_t bit_size, size_t bit_to_read,
        ebm::Endian endian, ebm::TypeRef target_type, ebm::TypeRef src_type) {
        if (is_nil(src_type)) {
            src_type = u8_type;
        }
        EBM_CAST(casted_new_bits, target_type, src_type, extracted);

        size_t final_shift = get_expr_shift(endian, bit_size, bits_processed, bit_to_read);

        auto result = casted_new_bits;

        if (final_shift != 0) {
            EBMU_INT_LITERAL(shift_val, final_shift);
            EBM_BINARY_OP(shifted_bits, ebm::BinaryOp::left_shift, target_type, result, shift_val);
            result = shifted_bits;
        }

        if (current_expr) {
            EBM_BINARY_OP(combined, ebm::BinaryOp::bit_or, target_type, *current_expr, result);
            return combined;
        }
        return result;
    }

    // 値から、エンディアンを考慮して8ビットに収まる範囲を取り出す
    // expr_shift = big ? bit_size - bits_processed - bit_to_read : bits_processed
    // byte_shift = big ? (8 - bit_to_read - bit_offset) : bit_offset;
    // lsb_mask = (1 << bit_to_read) - 1
    // optimize_shift = expr_shift >= byte_shift
    // shift = optimize_shift ? expr_shift - byte_shift : expr_shift
    // mask = optimize_shift ? lsb_mask << byte_shift : lsb_mask
    // uint8((source_expr >> shift) & mask)
    expected<std::pair<ebm::ExpressionRef, bool>> BitManipulator::extractBitsFromExpression(
        ebm::ExpressionRef source_expr,
        size_t bits_processed, size_t bit_size, size_t bit_offset, size_t bit_to_read,
        ebm::Endian endian, ebm::TypeRef target_type, bool should_optimize, bool cast_u8) {
        const size_t expr_shift = get_expr_shift(endian, bit_size, bits_processed, bit_to_read);
        const size_t byte_shift = get_byte_shift(endian, bit_offset, bit_to_read);
        const bool optimize_shift = expr_shift >= byte_shift;
        const size_t shift = optimize_shift && should_optimize ? expr_shift - byte_shift : expr_shift;
        const std::uint8_t lsb_mask = mask_value(bit_to_read);
        const std::uint8_t mask = optimize_shift ? lsb_mask << byte_shift : lsb_mask;

        auto result = source_expr;

        if (shift != 0) {
            EBMU_INT_LITERAL(shift_val, shift);
            EBM_BINARY_OP(shifted_bits, ebm::BinaryOp::right_shift, target_type, result, shift_val);
            result = shifted_bits;
        }

        EBMU_INT_LITERAL(mask_val, mask);
        EBM_BINARY_OP(masked, ebm::BinaryOp::bit_and, target_type, result, mask_val);
        if (!cast_u8) {
            return std::pair{masked, optimize_shift && should_optimize};
        }
        EBM_CAST(casted, u8_type, target_type, masked);
        return std::pair{casted, optimize_shift && should_optimize};
    }
    // 1バイトに抽出されたビットを正しい位置に追加する
    // byte_shift = big ? (8 - bit_to_read - bit_offset) : bit_offset
    // shift = optimized_shift ? 0 : byte_shift
    // mask = ((1 << bit_to_read) - 1)
    // tmp_buffer[offset] = tmp_buffer[offset] | (extracted << shift)
    expected<ebm::StatementRef> BitManipulator::appendBitsToByte(size_t byte_offset, size_t bit_offset, size_t bit_to_read, ebm::Endian endian,
                                                                 ebm::ExpressionRef extracted, bool optimized_shift) {
        EBMU_INT_LITERAL(offset, byte_offset);
        EBM_INDEX(byte_val, u8_type, tmp_buffer_, offset);

        // マスクを作成: (1 << bit_to_read) - 1 で bit_to_read 個の1を作り、正しい位置へシフト
        const auto byte_shift = get_byte_shift(endian, bit_offset, bit_to_read);
        const auto shift = optimized_shift ? 0 : byte_shift;

        assert(bit_to_read <= 8);
        ebm::ExpressionRef result = extracted;
        // 元の位置におくために左シフト
        if (shift > 0) {
            EBMU_INT_LITERAL(shift_val, shift);
            EBM_BINARY_OP(shifted, ebm::BinaryOp::left_shift, u8_type, result, shift_val);
            result = shifted;
        }
        // 1バイトまるごとではない場合,bit_orする
        if (bit_to_read != 8 && bit_offset != 0) {
            EBM_BINARY_OP(or_, ebm::BinaryOp::bit_or, u8_type, byte_val, result);
            result = or_;
        }
        EBM_ASSIGNMENT(assign, byte_val, result);
        return assign;
    }
}  // namespace ebmgen