/*license*/
#include <optional>
#include "ebm/extended_binary_module.hpp"
#include "helper.hpp"
#include "../converter.hpp"

namespace ebmgen {
    expected<ebm::StatementRef> DecoderConverter::decode_multi_byte_int_with_fixed_array(ebm::StatementRef io_ref, ebm::StatementRef field_ref, size_t n, ebm::IOAttribute endian, ebm::ExpressionRef to, ebm::TypeRef cast_to) {
        COMMON_BUFFER_SETUP(EBM_READ_DATA, read_ref, io_ref, field_ref, ebm::ArrayAnnotation::read_temporary);

        if (n == 1) {  // special case for 1 byte
            EBM_INDEX(array_index, u8_t, buffer, zero);
            EBM_CAST(cast_ref, cast_to, value_type, array_index);
            EBM_ASSIGNMENT(assign, to, cast_ref);
            ebm::Block block;
            block.container.reserve(3);
            append(block, buffer_def);
            append(block, read_ref);
            append(block, assign);
            EBM_BLOCK(block_ref, std::move(block));
            return block_ref;
        }

        EBMU_INT_LITERAL(xFF, 0xff);
        // value_holder = value_type(0)
        // buffer = [0] * n
        // read(buffer)
        // for counter in 0..len:
        //   if little endian
        //     shift_index = counter
        //   else
        //     // len - 1 can be constant
        //     shift_index = len - 1 - counter
        //   value_holder |= (value_type(buffer[counter]) << (8 * shift_index))
        // to = (may cast) value_holder
        auto generate_mask = [&](std::optional<ebm::ExpressionRef> prev, size_t i, size_t shift_index) -> expected<ebm::ExpressionRef> {
            EBMU_INT_LITERAL(shift, shift_index * 8);
            EBMU_INT_LITERAL(idx, i);
            EBM_INDEX(array_index, u8_t, buffer, idx);
            EBM_CAST(casted, value_type, u8_t, array_index);
            EBM_BINARY_OP(shifted, ebm::BinaryOp::left_shift, value_type, casted, shift);
            if (prev) {
                EBM_BINARY_OP(combined, ebm::BinaryOp::bit_or, value_type, *prev, shifted);
                return combined;
            }
            return shifted;
        };

        auto do_it = add_endian_specific(
            ctx,
            endian,
            [&] -> expected<ebm::StatementRef> {
                std::optional<ebm::ExpressionRef> prev;
                for (size_t i = 0; i < n; i++) {
                    MAYBE(mask, generate_mask(prev, i, i));
                    prev = mask;
                }
                EBM_CAST(cast_ref, cast_to, value_type, *prev);
                EBM_ASSIGNMENT(assign, to, cast_ref);
                EBM_ENDIAN_CONVERT(conv, ebm::StatementKind::ARRAY_TO_INT, ebm::Endian::little, buffer, to, assign);
                return conv;
            },
            [&] -> expected<ebm::StatementRef> {
                std::optional<ebm::ExpressionRef> prev;
                for (size_t i = 0; i < n; i++) {
                    size_t shift_index = n - 1 - i;
                    MAYBE(mask, generate_mask(prev, i, shift_index));
                    prev = mask;
                }
                EBM_CAST(cast_ref, cast_to, value_type, *prev);
                EBM_ASSIGNMENT(assign, to, cast_ref);
                EBM_ENDIAN_CONVERT(conv, ebm::StatementKind::ARRAY_TO_INT, ebm::Endian::big, buffer, to, assign);
                return conv;
            });
        if (!do_it) {
            return unexpect_error(std::move(do_it.error()));
        }

        ebm::Block block;
        block.container.reserve(3);
        append(block, buffer_def);
        append(block, read_ref);
        append(block, *do_it);
        EBM_BLOCK(block_ref, std::move(block));
        return block_ref;
    }

    expected<void> DecoderConverter::decode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& ity, ebm::ExpressionRef base_ref) {
        MAYBE(attr, ctx.state().get_io_attribute(ebm::Endian(ity->endian), ity->is_signed));
        io_desc.attribute = attr;
        io_desc.size = get_size(*ity->bit_size);
        if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
            MAYBE(multi_byte_int, decode_multi_byte_int_with_fixed_array(io_desc.io_ref, from_weak(io_desc.field), *ity->bit_size / 8, attr, base_ref, io_desc.data_type));
            io_desc.attribute.has_lowered_statement(true);
            io_desc.lowered_statement(make_lowered_statement(ebm::LoweringIOType::INT_TO_BYTE_ARRAY, multi_byte_int));
        }
        return {};
    }

    expected<void> DecoderConverter::decode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& fty, ebm::ExpressionRef base_ref) {
        MAYBE(attr, ctx.state().get_io_attribute(ebm::Endian(fty->endian), false));
        io_desc.attribute = attr;
        io_desc.size = get_size(*fty->bit_size);
        if (io_desc.size.unit == ebm::SizeUnit::BYTE_FIXED) {
            MAYBE(multi_byte_int, decode_multi_byte_int_with_fixed_array(io_desc.io_ref, from_weak(io_desc.field), *fty->bit_size / 8, attr, base_ref, io_desc.data_type));
            io_desc.attribute.has_lowered_statement(true);
            io_desc.lowered_statement(make_lowered_statement(ebm::LoweringIOType::INT_TO_BYTE_ARRAY, multi_byte_int));
        }
        return {};
    }

    expected<void> DecoderConverter::decode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& ety, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field) {
        if (auto locked_enum = ety->base.lock()) {
            if (locked_enum->base_type) {
                EBMA_CONVERT_TYPE(to_ty, locked_enum->base_type);
                EBM_DEFAULT_VALUE(default_, to_ty);
                EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_var, to_ty, default_);
                MAYBE(decode_info, decode_field_type(locked_enum->base_type, tmp_var, nullptr, from_weak(io_desc.field)));
                EBMA_ADD_STATEMENT(decode_stmt, std::move(decode_info));
                EBM_CAST(casted, io_desc.data_type, to_ty, tmp_var);
                EBM_ASSIGNMENT(assign, base_ref, casted);
                ebm::Block block;
                append(block, tmp_var_def);
                append(block, decode_stmt);
                append(block, assign);
                EBM_BLOCK(block_ref, std::move(block));
                auto io_data = ctx.repository().get_statement(decode_stmt)->body.read_data();
                io_desc.attribute = io_data->attribute;
                io_desc.size = io_data->size;
                io_desc.attribute.has_lowered_statement(true);
                io_desc.lowered_statement(make_lowered_statement(ebm::LoweringIOType::ENUM_UNDERLYING_TO_INT, block_ref));
            }
            else {
                return unexpect_error("EnumType without base type cannot be used in encoding");
            }
        }
        else {
            return unexpect_error("EnumType has no base enum");
        }
        return {};
    }

    expected<void> DecoderConverter::decode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& aty, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field) {
        EBMA_CONVERT_TYPE(element_type, aty->element_type);
        const auto elem_body = ctx.repository().get_type(element_type);
        const auto is_byte = elem_body->body.kind == ebm::TypeKind::UINT && elem_body->body.size()->value() == 8;
        auto fixed_unit = [is_byte]() -> ebm::SizeUnit {
            return is_byte ? ebm::SizeUnit::BYTE_FIXED : ebm::SizeUnit::ELEMENT_FIXED;
        };
        auto dynamic_unit = [is_byte]() -> ebm::SizeUnit {
            return is_byte ? ebm::SizeUnit::BYTE_DYNAMIC : ebm::SizeUnit::ELEMENT_DYNAMIC;
        };
        auto set_fixed_size = [&](size_t n) -> expected<void> {
            MAYBE(size, make_fixed_size(n, fixed_unit()));
            io_desc.size = size;
            return {};
        };
        auto set_dynamic_size = [&](ebm::ExpressionRef n) -> expected<void> {
            MAYBE(size, make_dynamic_size(n, dynamic_unit()));
            io_desc.size = size;
            return {};
        };
        std::optional<ebm::ExpressionRef> length;
        std::optional<ebm::StatementRef> cond_loop;
        auto underlying_decoder = [&](std::optional<ebm::ExpressionRef> fixed_iter) -> expected<ebm::StatementRef> {
            ebm::Block block;
            if (fixed_iter) {
                EBM_INDEX(array_index, element_type, base_ref, *fixed_iter);
                EBM_DEFINE_VARIABLE(element, {}, element_type, array_index, ebm::VariableDeclKind::MUTABLE, true);
                MAYBE(decode_info, decode_field_type(aty->element_type, element, nullptr, from_weak(io_desc.field)));
                EBMA_ADD_STATEMENT(decode_stmt, std::move(decode_info));
                append(block, element_def);
                append(block, decode_stmt);
            }
            else {
                EBM_DEFAULT_VALUE(new_, element_type);
                EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_var, element_type, new_);
                MAYBE(decode_info, decode_field_type(aty->element_type, tmp_var, nullptr, from_weak(io_desc.field)));
                EBMA_ADD_STATEMENT(decode_stmt, std::move(decode_info));
                EBM_APPEND(appended, base_ref, tmp_var);
                append(block, tmp_var_def);
                append(block, decode_stmt);
                append(block, appended);
            }
            EBM_BLOCK(block_ref, std::move(block));
            return block_ref;
        };
        if (aty->length_value) {
            EBMU_INT_LITERAL(imm_len, *aty->length_value);
            MAYBE_VOID(ok, set_fixed_size(*aty->length_value));
            length = imm_len;
        }
        else if (ast::is_any_range(aty->length)) {
            if (field) {
                if (is_alignment_vector(field)) {
                    MAYBE(req_size, get_alignment_requirement(ctx, *field->arguments->alignment_value / 8, ebm::StreamType::INPUT, io_desc.io_ref));
                    MAYBE_VOID(ok, set_dynamic_size(req_size));
                    length = req_size;
                }
                else if (field->follow == ast::Follow::end ||
                         (field->arguments && field->arguments->sub_byte_length)  // this means that the field is a sub-byte field
                ) {
                    if (!is_byte) {
                        const auto single_byte = get_size(8);
                        EBM_CAN_READ_STREAM(can_read, io_desc.io_ref, ebm::StreamType::INPUT, single_byte);
                        MAYBE(element_decoder, underlying_decoder(std::nullopt));
                        EBM_WHILE_LOOP(loop, can_read, element_decoder);
                        cond_loop = loop;
                    }

                    EBM_GET_REMAINING_BYTES(remain_size, ebm::StreamType::INPUT, io_desc.io_ref);
                    MAYBE_VOID(ok, set_dynamic_size(remain_size));
                }
                else if (field->eventual_follow == ast::Follow::end && field->follow == ast::Follow::fixed) {
                    auto tail = field->belong_struct.lock()->fixed_tail_size / 8;
                    EBMU_INT_LITERAL(last, tail);
                    EBM_GET_REMAINING_BYTES(remain_bytes, ebm::StreamType::INPUT, io_desc.io_ref);

                    if (!is_byte) {
                        EBMU_BOOL_TYPE(bool_type);
                        EBM_BINARY_OP(cond, ebm::BinaryOp::greater, bool_type, remain_bytes, last);
                        MAYBE(element_decoder, underlying_decoder(std::nullopt));
                        EBM_WHILE_LOOP(loop, cond, element_decoder);
                        cond_loop = loop;
                    }

                    EBMU_COUNTER_TYPE(counter_type);
                    EBM_BINARY_OP(remain_size, ebm::BinaryOp::sub, counter_type, remain_bytes, last);
                    MAYBE_VOID(ok, set_dynamic_size(remain_size));
                }
                else if (field->follow == ast::Follow::constant) {
                    auto next = field->next.lock();
                    if (!next) {
                        return unexpect_error("Invalid next field");
                    }
                    auto str = ast::cast_to<ast::StrLiteralType>(next->field_type);
                    MAYBE(candidate, decode_base64(str->strong_ref));
                    EBMU_U8_N_ARRAY(array_type, candidate.size(), ebm::ArrayAnnotation::read_temporary);
                    EBM_DEFAULT_VALUE(array_default, array_type);
                    EBM_DEFINE_ANONYMOUS_VARIABLE(temporary_read_buffer, array_type, array_default);

                    MAYBE(size, make_fixed_size(candidate.size(), ebm::SizeUnit::BYTE_FIXED));

                    auto peek_io = make_io_data(io_desc.io_ref, from_weak(io_desc.field), temporary_read_buffer, array_type, io_desc.attribute, size);
                    peek_io.attribute.is_peek(true);
                    EBM_READ_DATA(temporary_read, std::move(peek_io));

                    ebm::ExpressionRef cond;
                    EBMU_BOOL_TYPE(bool_type);
                    for (size_t i = 0; i < candidate.size(); i++) {
                        EBMU_INT_LITERAL(i_ref, i);
                        EBMU_INT_LITERAL(literal, static_cast<unsigned char>(candidate[i]));
                        EBM_INDEX(array_index, array_type, temporary_read_buffer, i_ref);
                        EBM_BINARY_OP(check, ebm::BinaryOp::equal, bool_type, array_index, literal);
                        if (i == 0) {
                            cond = check;
                        }
                        else {
                            EBM_BINARY_OP(new_cond, ebm::BinaryOp::logical_and, bool_type, cond, check);
                            cond = new_cond;
                        }
                    }
                    if (is_nil(cond)) {
                        return unexpect_error("Condition expression is empty");
                    }
                    MAYBE(loop_id, ctx.repository().new_statement_id());
                    EBM_BREAK(break_stmt, loop_id);

                    EBM_IF_STATEMENT(if_stmt, cond, break_stmt, {});
                    MAYBE(data_decoder, underlying_decoder(std::nullopt));

                    ebm::LoopStatement loop_stmt;
                    loop_stmt.loop_type = ebm::LoopType::INFINITE;
                    // for:
                    //    temporary_read_buffer = peek(candidate.size())
                    //    if temporary_read_buffer == candidate:
                    //        break
                    //    data = decode_element()
                    //    append(base_ref, data)
                    ebm::Block loop_body;
                    append(loop_body, temporary_read_buffer_def);
                    append(loop_body, temporary_read);
                    append(loop_body, if_stmt);
                    append(loop_body, data_decoder);
                    EBM_BLOCK(loop_body_ref, std::move(loop_body));
                    loop_stmt.body = loop_body_ref;

                    auto loop = make_loop(std::move(loop_stmt));

                    EBMA_ADD_STATEMENT(loop_ref, loop_id, std::move(loop));
                    cond_loop = loop_ref;
                    io_desc.size.unit = ebm::SizeUnit::DYNAMIC;
                }
                else {
                    return unexpect_error("Invalid follow type: {}, {}", to_string(field->follow), to_string(field->eventual_follow));
                }
            }
            else {
                return unexpect_error("expected field but got nullptr");
            }
        }
        else {
            EBMA_CONVERT_EXPRESSION(length_v, aty->length);
            auto body = ctx.repository().get_expression(length_v);
            if (!body) {
                return unexpect_error("Array length expression is not found");
            }
            EBMU_COUNTER_TYPE(counter_type);
            EBM_CAST(casted_length, counter_type, body->body.type, length_v);
            MAYBE_VOID(ok, set_dynamic_size(casted_length));
            length = casted_length;
        }
        if (length && cond_loop) {
            return unexpect_error("Both length and cond_loop are set, which is not allowed; maybe BUG");
        }
        // For byte arrays (is_byte == true), the size is already set correctly
        // and no element-by-element loop is needed. Code generators handle
        // bytes IO as a primitive bulk operation.
        // Exception: Follow::constant sets cond_loop with DYNAMIC size (peek-until-marker loop).
        if (is_byte && !cond_loop) {
            return {};
        }
        if (cond_loop) {
            io_desc.attribute.has_lowered_statement(true);
            io_desc.lowered_statement(make_lowered_statement(ebm::LoweringIOType::ARRAY_FOR_EACH, *cond_loop));
        }
        else if (length) {
            EBMU_COUNTER_TYPE(counter_type);
            EBM_COUNTER_LOOP_START(counter);
            MAYBE(decode_info, underlying_decoder(aty->length_value.has_value() ? std::make_optional(counter) : std::nullopt));
            EBM_COUNTER_LOOP_END(lowered_stmt, counter, *length, decode_info);
            io_desc.attribute.has_lowered_statement(true);
            io_desc.lowered_statement(make_lowered_statement(ebm::LoweringIOType::ARRAY_FOR_EACH, lowered_stmt));
        }
        else {
            return unexpect_error("Neither length nor cond_loop is set, which is not allowed; maybe BUG");
        }
        return {};
    }

    expected<void> compare_string_array(ConverterContext& ctx, ebm::Block& block, ebm::ExpressionRef n_array, const std::string& candidate) {
        EBMU_U8(u8_t);
        for (size_t i = 0; i < candidate.size(); i++) {
            EBMU_INT_LITERAL(i_ref, i);
            EBMU_INT_LITERAL(literal, static_cast<unsigned char>(candidate[i]));
            EBM_INDEX(array_index, u8_t, n_array, i_ref);
            EBM_BINARY_OP(check, ebm::BinaryOp::equal, u8_t, array_index, literal);
            MAYBE(assert_stmt, assert_statement(ctx, check));
            append(block, assert_stmt);
        }
        return {};
    }

    expected<void> DecoderConverter::decode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref) {
        MAYBE(candidate, decode_base64(typ->strong_ref));

        EBMU_U8_N_ARRAY(u8_n_array, candidate.size(), ebm::ArrayAnnotation::read_temporary);
        EBM_DEFAULT_VALUE(new_obj_ref, u8_n_array);
        EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);

        MAYBE(io_size, make_fixed_size(candidate.size(), ebm::SizeUnit::BYTE_FIXED));
        io_desc.size = io_size;
        EBM_READ_DATA(read_ref, make_io_data(io_desc.io_ref, from_weak(io_desc.field), buffer, u8_n_array, io_desc.attribute, io_size));

        ebm::Block block;
        append(block, buffer_def);
        append(block, read_ref);
        MAYBE_VOID(ok, compare_string_array(ctx, block, buffer, candidate));
        EBM_BLOCK(block_ref, std::move(block));
        io_desc.attribute.has_lowered_statement(true);
        io_desc.lowered_statement(make_lowered_statement(ebm::LoweringIOType::STRING_FOR_EACH, block_ref));
        return {};
    }

    expected<void> DecoderConverter::decode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field) {
        auto base = typ->base.lock();
        auto fmt = ast::as<ast::Format>(base);
        if (!fmt) {
            return unexpect_error("Struct type must have a format");
        }
        if (typ->bit_size && !(fmt->encode_fn.lock() || fmt->decode_fn.lock())) {
            auto size = get_size(*typ->bit_size);
            io_desc.size = size;
        }
        else {
            io_desc.size = ebm::Size{.unit = ebm::SizeUnit::DYNAMIC};
        }
        ebm::CallDesc call_desc;
        // force convert encode and decode functions
        {
            auto normal = ctx.state().set_current_generate_type(GenerateType::Normal);
            EBMA_CONVERT_STATEMENT(ok, base);
        }
        MAYBE(par_encdec, ctx.state().get_format_encode_decode(base));
        MAYBE(cur_encdec, ctx.state().get_format_encode_decode(ctx.state().get_current_node()));

        ctx.state().add_propagated_io_input_desc_hierarchy(cur_encdec.decoder_input_type, par_encdec.decoder_input_type);

        EBM_MEMBER_ACCESS(dec_access, par_encdec.decode_type, base_ref, par_encdec.decode);
        call_desc.callee = dec_access;

        MAYBE(dec_in_def, ctx.repository().get_expression(cur_encdec.decoder_input));
        EBM_AS_ARG(dec_in_arg, dec_in_def.body.type, cur_encdec.decoder_input);
        append(call_desc.arguments, dec_in_arg);
        for (auto& st : par_encdec.state_variables) {
            for (auto& cur_st : cur_encdec.state_variables) {
                if (cur_st.ast_field == st.ast_field) {
                    MAYBE(expr, ctx.repository().get_expression(cur_st.dec_var_expr));
                    EBM_AS_ARG(as_arg, expr.body.type, cur_st.dec_var_expr);
                    append(call_desc.arguments, as_arg);
                    break;
                }
            }
        }

        // TODO: add arguments
        auto func = ctx.state().get_current_function_id();
        MAYBE(init_field, make_field_init_check(ctx, base_ref, false, func));
        MAYBE(typ_ref, get_decoder_return_type(ctx));
        EBM_CALL(call_ref, typ_ref, std::move(call_desc));
        EBM_DEFINE_VARIABLE(result, {}, typ_ref, call_ref, ebm::VariableDeclKind::IMMUTABLE, false);
        EBM_IS_ERROR(is_error, result);
        EBM_ERROR_RETURN(error_return, result, ctx.state().get_current_function_id(), from_weak(io_desc.field));
        EBM_IF_STATEMENT(if_stmt, is_error, error_return, {});

        ebm::Block block;
        block.container.reserve(3);
        append(block, init_field);
        append(block, result_def);
        append(block, if_stmt);
        EBM_BLOCK(block_ref, std::move(block));

        io_desc.attribute.has_lowered_statement(true);
        io_desc.lowered_statement(make_lowered_statement(ebm::LoweringIOType::STRUCT_CALL, block_ref));
        return {};
    }

    expected<ebm::StatementBody> DecoderConverter::decode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field, ebm::StatementRef field_ref) {
        if (auto ity = ast::as<ast::IdentType>(typ)) {
            return decode_field_type(ity->base.lock(), base_ref, field, field_ref);
        }
        MAYBE(cur_encdec, ctx.state().get_format_encode_decode(ctx.state().get_current_node()));

        EBMA_CONVERT_TYPE(typ_ref, typ, field);
        ebm::IOData io_desc = make_io_data(cur_encdec.decoder_input_def, field_ref, base_ref, typ_ref, ebm::IOAttribute{}, ebm::Size{});

        if (auto ity = ast::as<ast::IntType>(typ)) {
            MAYBE_VOID(ok, decode_int_type(io_desc, ast::cast_to<ast::IntType>(typ), base_ref));
        }
        else if (auto fty = ast::as<ast::FloatType>(typ)) {
            MAYBE_VOID(ok, decode_float_type(io_desc, ast::cast_to<ast::FloatType>(typ), base_ref));
        }
        else if (auto str_lit = ast::as<ast::StrLiteralType>(typ)) {
            MAYBE_VOID(ok, decode_str_literal_type(io_desc, ast::cast_to<ast::StrLiteralType>(typ), base_ref));
        }
        else if (auto ety = ast::as<ast::EnumType>(typ)) {
            MAYBE_VOID(ok, decode_enum_type(io_desc, ast::cast_to<ast::EnumType>(typ), base_ref, field));
        }
        else if (auto aty = ast::as<ast::ArrayType>(typ)) {
            MAYBE_VOID(ok, decode_array_type(io_desc, ast::cast_to<ast::ArrayType>(typ), base_ref, field));
        }
        else if (auto sty = ast::as<ast::StructType>(typ)) {
            MAYBE_VOID(ok, decode_struct_type(io_desc, ast::cast_to<ast::StructType>(typ), base_ref, field));
        }
        else {
            return unexpect_error("Unsupported type for decoding: {}", node_type_to_string(typ->node_type));
        }
        assert(io_desc.size.unit != ebm::SizeUnit::UNKNOWN);
        return make_read_data(std::move(io_desc));
    }

}  // namespace ebmgen
