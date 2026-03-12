/*license*/
#include <bmgen/fallback.hpp>

namespace rebgn {

    Error assign_to_target(Module& m,
                           auto&& op, auto&& get_nbit_typ, auto&& new_var,
                           Varint buffer, auto&& offset_fn, Varint len,
                           Varint value, EndianExpr endian, size_t bit_size) {
        BM_IMMEDIATE(op, imm0, 0);
        if (bit_size == 8) {
            BM_INDEX(op, array_index, buffer, imm0);
            if (endian.sign()) {
                BM_ERROR_WRAP(u8_t, error, (get_nbit_typ(8, false)));
                BM_ERROR_WRAP(dst_t, error, (get_nbit_typ(bit_size, endian.sign())));
                BM_CAST(op, cast, dst_t, u8_t, array_index, CastType::SIGNED_TO_UNSIGNED);
                BM_ASSIGN(op, assign, value, cast, null_varint, nullptr);
            }
            else {
                // no need to cast
                BM_ASSIGN(op, assign, value, array_index, null_varint, nullptr);
            }
            return none;
        }

        BM_ERROR_WRAP(ref, error, (get_nbit_typ(64, false)));
        BM_ERROR_WRAP(counter, error, (new_var(ref, imm0)));

        BM_BINARY(op, cmp_id, BinaryOp::less, counter, len);
        auto assign = [&](Varint shift_index) {
            BM_ERROR_WRAP(offset, error, (offset_fn(counter)));
            BM_INDEX(op, array_index, buffer, offset);
            BM_ERROR_WRAP(u8_t, error, (get_nbit_typ(8, false)));
            BM_ERROR_WRAP(dst_t, error, (get_nbit_typ(bit_size, endian.sign())));
            BM_CAST(op, cast, dst_t, u8_t, array_index, CastType::SMALL_INT_TO_LARGE_INT);
            BM_IMMEDIATE(op, imm8, 8);
            BM_BINARY(op, shift, BinaryOp::mul, shift_index, imm8);
            BM_BINARY(op, shifted, BinaryOp::left_logical_shift, cast, shift);
            BM_BINARY(op, ored, BinaryOp::bit_or, value, shifted);
            BM_ASSIGN(op, assign, value, ored, null_varint, nullptr);
            return none;
        };

        // if bit_size != 8
        //   base_offset = 0
        //   for counter in range(len):
        //     offset = base_offset + counter
        //     if little endian
        //       shift_index = counter
        //     else
        //       // len - 1 can be constant
        //       shift_index = len - 1 - counter
        //     value = value | (value_type(buffer[offset]) << (8 * shift_index))

        auto err = add_endian_specific(
            m, endian, op,
            [&] {  // on little endian
                return loop(op, cmp_id, counter, [&] { return assign(counter); });
            },
            [&] {  // on big endian
                return loop(op, cmp_id, counter, [&] {
                    BM_IMMEDIATE(op, len_minus_1, bit_size / 8 - 1);
                    BM_BINARY(op, shift_index, BinaryOp::sub, len_minus_1, counter);
                    return assign(shift_index);
                });
            });
        if (err) {
            return err;
        }

        return none;
    }

    Error decode_single_int(Module& m, auto&& op, auto&& new_array, auto&& new_var, Varint value, size_t bit_size, EndianExpr endian, Varint belong) {
        std::optional<StorageRef> u8_typ;
        auto get_nbit_typ = nbit_type_getter(m, u8_typ);

        BM_ERROR_WRAP(buffer, error, (new_array(bit_size / 8)));
        BM_IMMEDIATE(op, len, bit_size / 8);

        BM_DECODE_INT_VEC_FIXED(op, buffer, len, endian, 8, belong, bit_size / 8);

        return assign_to_target(m, op, get_nbit_typ, new_var, buffer, [](Varint counter) -> expected<Varint> { return counter; }, len, value, endian, bit_size);
    }

    Error encode_single_int(Module& m, auto&& op, auto&& new_array, auto&& new_var, Varint value, size_t bit_size, EndianExpr endian, Varint belong) {
        std::optional<StorageRef> u8_typ;
        auto get_nbit_typ = nbit_type_getter(m, u8_typ);
        BM_ERROR_WRAP(buffer, error, (new_array(bit_size / 8)));
        BM_IMMEDIATE(op, len, bit_size / 8);
        BM_IMMEDIATE(op, imm0, 0);
        if (bit_size == 8) {
            BM_INDEX(op, array_index, buffer, imm0);
            if (endian.sign()) {
                BM_ERROR_WRAP(u8_t, error, (get_nbit_typ(8, false)));
                BM_ERROR_WRAP(src_t, error, (get_nbit_typ(bit_size, endian.sign())));
                BM_CAST(op, cast, u8_t, src_t, value, CastType::SIGNED_TO_UNSIGNED);
                BM_ASSIGN(op, assign, array_index, cast, null_varint, nullptr);
            }
            else {
                // no need to cast
                BM_ASSIGN(op, assign, array_index, value, null_varint, nullptr);
            }
        }
        else {
            BM_ERROR_WRAP(ref, error, (get_nbit_typ(64, false)));

            BM_ERROR_WRAP(counter, error, (new_var(ref, imm0)));

            BM_BINARY(op, cmp_id, BinaryOp::less, counter, len);

            auto assign = [&](Varint shift_index) {
                BM_IMMEDIATE(op, imm8, 8);
                BM_BINARY(op, shift, BinaryOp::mul, shift_index, imm8);
                BM_BINARY(op, shifted, BinaryOp::right_logical_shift, value, shift);
                BM_IMMEDIATE(op, immFF, 0xff);
                BM_BINARY(op, masked, BinaryOp::bit_and, shifted, immFF);
                BM_INDEX(op, array_index, buffer, counter);
                BM_ERROR_WRAP(u8_t, error, (get_nbit_typ(8, false)));
                BM_ERROR_WRAP(src_t, error, (get_nbit_typ(bit_size, endian.sign())));
                BM_CAST(op, cast, u8_t, src_t, masked, CastType::LARGE_INT_TO_SMALL_INT);
                BM_ASSIGN(op, assign, array_index, cast, null_varint, nullptr);
                return none;
            };

            // if little endian
            //   shift_index = counter
            // else
            //   // len - 1 can be constant
            //   shift_index = len - 1 - counter
            // buffer[counter] = (value >> (8 * shift_index)) & 0xff

            auto err = add_endian_specific(
                m, endian, op,
                [&] {  // on little endian
                    return loop(op, cmp_id, counter, [&] { return assign(counter); });
                },
                [&] {  // on big endian
                    return loop(op, cmp_id, counter, [&] {
                        BM_IMMEDIATE(op, len_minus_1, bit_size / 8 - 1);
                        BM_BINARY(op, shift_index, BinaryOp::sub, len_minus_1, counter);
                        return assign(shift_index);
                    });
                });
            if (err) {
                return err;
            }
        }

        BM_ENCODE_INT_VEC_FIXED(op, buffer, len, endian, 8, belong, bit_size / 8);
        return none;
    }

    Error fallback_encode_int(Module& m, Code& code, std::vector<Code>& new_code) {
        if (code.fallback()->value() != null_id) {
            return none;
        }
        NEW_CODE_OP(new_code);
        BM_NEW_ID(fallback_id, error, nullptr);
        code.fallback(fallback_id);
        op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
            m.ident(fallback_id);
        });

        encode_single_int(m, op, new_array, new_var, code.ref().value(), code.bit_size()->value(), code.endian().value(), code.belong().value());

        op(AbstractOp::END_FALLBACK, [&](Code& m) {});
        return none;
    }

    Error fallback_decode_int(Module& m, Code& code, std::vector<Code>& new_code) {
        if (code.fallback()->value() != null_id) {
            return none;
        }
        NEW_CODE_OP(new_code);
        BM_NEW_ID(fallback_id, error, nullptr);
        code.fallback(fallback_id);
        op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
            m.ident(fallback_id);
        });

        decode_single_int(m, op, new_array, new_var, code.ref().value(), code.bit_size()->value(), code.endian().value(), code.belong().value());

        op(AbstractOp::END_FALLBACK, [&](Code& m) {});
        return none;
    }

    Error fallback_int_vector(Module& m, Code& code, std::vector<Code>& new_code, bool encode) {
        if (code.fallback()->value() != null_id) {
            return none;
        }
        auto int_bit_size = code.bit_size()->value();
        if (int_bit_size == 8) {
            return none;  // no need to make fallback
        }
        NEW_CODE_OP(new_code);
        BM_NEW_ID(fallback_id, error, nullptr);
        code.fallback(fallback_id);
        op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
            m.ident(fallback_id);
        });

        auto base_ref = code.left_ref().value();
        auto endian = code.endian().value();
        auto belong = code.belong().value();
        auto len_ref = code.right_ref().value();

        std::optional<StorageRef> u8_typ;
        auto get_nbit_typ = nbit_type_getter(m, u8_typ);

        BM_ERROR_WRAP(counter_type, error, (get_nbit_typ(64, false)));
        BM_IMMEDIATE(op, imm0, 0);
        BM_ERROR_WRAP(counter, error, (new_var(counter_type, imm0)));

        BM_BINARY(op, cmp_id, BinaryOp::less, counter, len_ref);

        auto err = loop(op, len_ref, counter, [&] {
            BM_INDEX(op, index, base_ref, counter);
            return encode ? encode_single_int(m, op, new_array, new_var, index, int_bit_size, endian, belong)
                          : decode_single_int(m, op, new_array, new_var, index, int_bit_size, endian, belong);
        });
        if (err) {
            return err;
        }

        op(AbstractOp::END_FALLBACK, [&](Code& m) {});
        return none;
    }

    Error fallback_decode_int_vector_until_eof(Module& m, Code& code, std::vector<Code>& new_code) {
        if (code.fallback()->value() != null_id) {
            return none;
        }
        auto int_bit_size = code.bit_size()->value();
        if (int_bit_size == 8) {
            return none;  // no need to make fallback
        }
        NEW_CODE_OP(new_code);
        BM_NEW_ID(fallback_id, error, nullptr);
        code.fallback(fallback_id);
        op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
            m.ident(fallback_id);
        });

        auto base_ref = code.ref().value();
        auto endian = code.endian().value();
        auto belong = code.belong().value();

        std::optional<StorageRef> u8_typ;
        auto get_nbit_typ = nbit_type_getter(m, u8_typ);

        BM_ERROR_WRAP(tmp_buffer, error, (new_vector(null_varint)));

        op(AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF, [&](Code& c) {
            c.ref(tmp_buffer);
            c.endian(endian);
            c.bit_size(*varint(8));
            c.belong(belong);
        });

        BM_NEW_ID(vec_size, error, nullptr);

        op(AbstractOp::ARRAY_SIZE, [&](Code& c) {
            c.ident(vec_size);
            c.ref(tmp_buffer);
        });

        BM_IMMEDIATE(op, imm_size, int_bit_size / 8);
        BM_IMMEDIATE(op, imm0, 0);
        BM_BINARY(op, mod_is_zero, BinaryOp::mod, vec_size, imm_size);
        BM_BINARY(op, assert_expr, BinaryOp::equal, mod_is_zero, imm0);
        BM_REF(op, AbstractOp::ASSERT, assert_expr);
        BM_BINARY(op, len_expr, BinaryOp::div, vec_size, imm_size);

        BM_ERROR_WRAP(counter_type, error, (get_nbit_typ(64, false)));

        BM_ERROR_WRAP(len_ref, error, (new_var(counter_type, len_expr)));
        BM_ERROR_WRAP(counter, error, (new_var(counter_type, imm0)));

        op(AbstractOp::RESERVE_SIZE, [&](Code& c) {
            c.left_ref(base_ref);
            c.right_ref(len_ref);
            c.reserve_type(ReserveType::DYNAMIC);
        });

        BM_BINARY(op, cmp_id, BinaryOp::less, counter, len_ref);

        BM_IMMEDIATE(op, elem_size, int_bit_size / 8);

        auto err = loop(op, cmp_id, counter, [&] {
            BM_INDEX(op, index, base_ref, counter);
            auto err = assign_to_target(
                m, op, get_nbit_typ, new_var, tmp_buffer,
                [&](Varint counter) -> expected<Varint> {
                    BM_BINARY_UNEXPECTED(op, base_offset, BinaryOp::mul, counter, elem_size);
                    BM_BINARY_UNEXPECTED(op, offset, BinaryOp::add, base_offset, counter);
                    return offset;
                },
                elem_size, index, endian, int_bit_size);
            if (err) {
                return err;
            }
            return none;
        });
        if (err) {
            return err;
        }

        op(AbstractOp::END_FALLBACK, [&](Code& m) {});
        return none;
    }

    Error apply_encode_fallback(Module& m) {
        std::vector<Code> new_code;
        for (auto& code : m.code) {
            if (code.op == AbstractOp::ENCODE_INT) {
                auto err = fallback_encode_int(m, code, new_code);
                if (err) {
                    return err;
                }
            }
            else if (code.op == AbstractOp::DECODE_INT) {
                auto err = fallback_decode_int(m, code, new_code);
                if (err) {
                    return err;
                }
            }
            else if (code.op == AbstractOp::ENCODE_INT_VECTOR ||
                     code.op == AbstractOp::ENCODE_INT_VECTOR_FIXED) {
                auto err = fallback_int_vector(m, code, new_code, true);
                if (err) {
                    return err;
                }
            }
            else if (code.op == AbstractOp::DECODE_INT_VECTOR ||
                     code.op == AbstractOp::DECODE_INT_VECTOR_FIXED) {
                auto err = fallback_int_vector(m, code, new_code, false);
                if (err) {
                    return err;
                }
            }
            else if (code.op == AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF) {
                auto err = fallback_decode_int_vector_until_eof(m, code, new_code);
                if (err) {
                    return err;
                }
            }
        }
        m.code.insert_range(m.code.end(), std::move(new_code));
        return none;
    }
}  // namespace rebgn
