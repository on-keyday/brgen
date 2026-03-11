/*license*/
#include <bmgen/fallback.hpp>
#include <bmgen/bit.hpp>

namespace rebgn {
    Error expand_bit_operation(Module& m) {
        std::vector<Code> new_code;
        NEW_CODE_OP(new_code);
        bool on_bit_operation = false;
        EndianExpr endian{};
        PackedOpType packed_op = PackedOpType::FIXED;
        size_t bit_size = 0;
        Varint counter, target, tmp_array, read_bytes, belong;
        StorageRef target_type, nbit_typ;
        std::optional<StorageRef> u8_typ;

        auto get_nbit_typ = nbit_type_getter(m, u8_typ);
        // target = target | ((target_type(value) & src_type_bits) << shift_index)
        auto assign_to_target = [&](Varint ref, Varint mask, Varint shift_index, auto&& src_type, auto&& src_type_storage) {
            // cast to target type
            BM_ERROR_WRAP(dst_storage, error, (m.get_storage(target_type)));
            auto casted = add_assign_cast(m, op, &dst_storage, &src_type_storage, ref);
            if (!casted) {
                return casted.error();
            }
            BM_BINARY(op, and_, BinaryOp::bit_and, casted ? **casted : ref, mask);
            BM_BINARY(op, shift, BinaryOp::left_logical_shift, and_, shift_index);
            BM_BINARY(op, bit_or, BinaryOp::bit_or, target, shift);
            BM_ASSIGN(op, assign, target, bit_or, null_varint, nullptr);
            return none;
        };

        auto add_counter = [&](size_t i) {
            BM_IMMEDIATE(op, imm, i);
            BM_BINARY(op, add, BinaryOp::add, counter, imm);
            BM_ASSIGN(op, assign, counter, add, null_varint, nullptr);
            return none;
        };

        for (auto& code : m.code) {
            if (code.op == rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION ||
                code.op == rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION) {
                on_bit_operation = true;
                endian = code.endian().value();
                packed_op = code.packed_op_type().value();
                belong = code.belong().value();  // refer to bit field
                auto& maybe_type = m.code[m.ident_index_table[code.belong().value().value()]];
                if (maybe_type.op != rebgn::AbstractOp::DEFINE_BIT_FIELD) {
                    return error("Invalid packed operation");
                }
                auto type = m.get_storage(maybe_type.type().value());
                if (!type) {
                    return type.error();
                }

                auto new_target_type = maybe_type.type().value();
                auto new_bit_size = type.value().storages[0].size()->value();

                code.bit_size(*varint(new_bit_size));
                auto fallback_id = m.new_id(nullptr);
                if (!fallback_id) {
                    return fallback_id.error();
                }
                code.fallback(*fallback_id);

                op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
                    m.ident(*fallback_id);
                });
                {
                    auto new_target_var = new_default_var(new_target_type);
                    if (!new_target_var) {
                        return new_target_var.error();
                    }

                    auto n_bit = futils::binary::log2i(new_bit_size);
                    Storage storage{.type = StorageType::UINT};
                    storage.size(*varint(n_bit));
                    auto new_nbit_typ = m.get_storage_ref(Storages{.length = *varint(1), .storages = {storage}}, nullptr);
                    if (!new_nbit_typ) {
                        return new_nbit_typ.error();
                    }
                    BM_IMMEDIATE(op, zero, 0);
                    auto new_counter_var = new_var(*new_nbit_typ, zero);
                    if (!new_counter_var) {
                        return new_counter_var.error();
                    }
                    Varint new_tmp_array, new_read_bytes;
                    if (packed_op == PackedOpType::VARIABLE) {
                        auto new_array_var = new_array(new_bit_size / 8);
                        if (!new_array_var) {
                            return new_array_var.error();
                        }
                        new_tmp_array = *new_array_var;
                    }
                    if (code.op == rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION) {
                        if (packed_op == PackedOpType::FIXED) {
                            op(AbstractOp::DECODE_INT, [&](Code& m) {
                                m.ref(*new_target_var);
                                m.endian(endian);
                                m.bit_size(*varint(new_bit_size));
                                m.belong(belong);  // refer to bit field
                            });
                        }
                        else {
                            auto read_bytes_var = new_var(*new_nbit_typ, zero);
                            if (!read_bytes_var) {
                                return read_bytes_var.error();
                            }
                            new_read_bytes = *read_bytes_var;
                        }
                    }

                    nbit_typ = *new_nbit_typ;
                    counter = *new_counter_var;
                    target_type = new_target_type;
                    target = *new_target_var;
                    tmp_array = new_tmp_array;
                    read_bytes = new_read_bytes;
                    bit_size = new_bit_size;
                }

                op(AbstractOp::END_FALLBACK, [&](Code& m) {});
            }
            else if (code.op == rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION ||
                     code.op == rebgn::AbstractOp::END_DECODE_PACKED_OPERATION) {
                BM_NEW_ID(fallback_id, error, nullptr);
                code.fallback(fallback_id);
                op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
                    m.ident(fallback_id);
                });
                {
                    if (code.op == rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION) {
                        if (packed_op == PackedOpType::FIXED) {
                            BM_ENCODE_INT(op, target, endian, bit_size, belong);
                        }
                        else {
                            // counter / 8
                            BM_IMMEDIATE(op, eight, 8);
                            BM_BINARY(op, div, BinaryOp::div, counter, eight);
                            BM_IMMEDIATE(op, zero, 0);
                            // TODO(on-keyday): for now, only support power of 8 bit
                            BM_ERROR_WRAP(result_byte_count, error, (new_var(nbit_typ, div)));
                            BM_ERROR_WRAP(i_, error, (new_var(nbit_typ, zero)));
                            // i_ < result_byte_count
                            BM_BINARY(op, less, BinaryOp::less, i_, result_byte_count);
                            // while (i_ < result_byte_count)
                            BM_REF(op, AbstractOp::LOOP_CONDITION, less);
                            {
                                // on each loop
                                // on little endian:
                                //    tmp_array[i_] = u8((target >> (i_ * 8)) & 0xff)
                                // on big endian:
                                //    tmp_array[i_] = u8((target >> ((bit_size / 8 - 1 - i_) * 8)) & 0xff)
                                // on native endian, platform dependent
                                // on dynamic endian, dynamic variable dependent
                                auto assign_to_array = [&](Varint shift_index) {
                                    BM_BINARY(op, mul, BinaryOp::mul, shift_index, eight);
                                    BM_BINARY(op, shift, BinaryOp::right_logical_shift, target, mul);
                                    BM_IMMEDIATE(op, immFF, 0xff);
                                    BM_BINARY(op, and_, BinaryOp::bit_and, shift, immFF);
                                    BM_ERROR_WRAP(u8_typ, error, (get_nbit_typ(8, false)));
                                    BM_CAST(op, cast, u8_typ, target_type, and_, CastType::LARGE_INT_TO_SMALL_INT);
                                    BM_INDEX(op, array_index, tmp_array, i_);
                                    BM_ASSIGN(op, assign, array_index, cast, null_varint, nullptr);
                                    return none;
                                };
                                auto err = add_endian_specific(
                                    m, endian, op,
                                    [&]() {  // on little endian
                                        return assign_to_array(i_);
                                    },
                                    [&]() {  // on big endian
                                        BM_IMMEDIATE(op, shift_index_base, bit_size / 8 - 1);
                                        BM_BINARY(op, shift_index, BinaryOp::sub, shift_index_base, i_);
                                        return assign_to_array(shift_index);
                                    });
                                if (err) {
                                    return err;
                                }
                                BM_REF(op, AbstractOp::INC, i_);
                            }
                            BM_OP(op, AbstractOp::END_LOOP);
                            BM_ENCODE_INT_VEC_FIXED(op, tmp_array, result_byte_count, endian, 8, belong, bit_size / 8);
                        }
                    }
                }
                op(AbstractOp::END_FALLBACK, [&](Code& m) {});
                // new_code.push_back(code);  // copy
                on_bit_operation = false;
            }
            else if (on_bit_operation && code.op == rebgn::AbstractOp::ENCODE_INT) {
                auto fallback_id = m.new_id(nullptr);
                if (!fallback_id) {
                    return fallback_id.error();
                }
                code.fallback(*fallback_id);
                op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
                    m.ident(*fallback_id);
                });
                auto enc_bit_size = code.bit_size()->value();
                auto enc_endian = code.endian().value();
                BM_ERROR_WRAP(src_type, error, (get_nbit_typ(enc_bit_size, enc_endian.sign())));
                BM_ERROR_WRAP(src_type_storage, error, (m.get_storage(src_type)));
                BM_IMMEDIATE(op, bit_mask, safe_left_shift(1, enc_bit_size) - 1);

                // on little endian:
                //   // fill from lsb
                //   target = target | (target_type(value) << counter)
                //   counter += bit_size
                // on big endian:
                //   // fill from msb
                //   counter += bit_size
                //   target = target | (target_type(value) << (nbit - counter))
                // on native endian, platform dependent
                // on dynamic endian, dynamic variable dependent
                auto err = add_endian_specific(
                    m, endian, op,
                    [&]() {
                        auto err = assign_to_target(code.ref().value(), bit_mask, counter, src_type, src_type_storage);
                        if (err) {
                            return err;
                        }
                        return add_counter(enc_bit_size);
                    },
                    [&]() {
                        auto err = add_counter(enc_bit_size);
                        if (err) {
                            return err;
                        }
                        BM_IMMEDIATE(op, bit_size_, bit_size);
                        BM_BINARY(op, sub, BinaryOp::sub, bit_size_, counter);
                        return assign_to_target(code.ref().value(), bit_mask, sub, src_type, src_type_storage);
                    });
                if (err) {
                    return err;
                }
                op(AbstractOp::END_FALLBACK, [&](Code& m) {});
            }
            else if (on_bit_operation && code.op == rebgn::AbstractOp::DECODE_INT) {
                BM_NEW_ID(fallback_id, error, nullptr);
                code.fallback(fallback_id);
                op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
                    m.ident(fallback_id);
                });
                auto dec_bit_size = code.bit_size()->value();
                auto dec_endian = code.endian().value();
                BM_ERROR_WRAP(src_type, error, (get_nbit_typ(dec_bit_size, dec_endian.sign())));
                BM_ERROR_WRAP(src_type_storage, error, (m.get_storage(src_type)));

                if (packed_op == PackedOpType::VARIABLE) {
                    // consumed_bytes = (counter + dec_bit_size + 7) / 8
                    // if (read_bytes < consumed_bytes)
                    //   for (i_ = read_bytes; i_ < consumed_bytes; i_++)
                    //     tmp_array[i_] = decode_u8()
                    //     on little endian:
                    //       target = target | (target_type(tmp_array[i_]) << (i_ * 8))
                    //     on big endian:
                    //       target = target | (target_type(tmp_array[i_]) << ((bit_size / 8 - i_ - 1) * 8))
                    //     on native endian, platform dependents (same as either little or big endian)
                    //     on dynamic endian, dynamic variable dependent (same as either little or big endian)
                    //   read_bytes = consumed_bytes // assign consumed_bytes to read_bytes
                    // end if

                    BM_IMMEDIATE(op, imm7, 7);
                    BM_IMMEDIATE(op, imm_size, dec_bit_size);
                    BM_IMMEDIATE(op, imm8, 8);
                    BM_BINARY(op, add, BinaryOp::add, counter, imm_size);
                    BM_BINARY(op, add2, BinaryOp::add, add, imm7);
                    BM_BINARY(op, div, BinaryOp::div, add2, imm8);
                    BM_ERROR_WRAP(consumed_bytes, error, (new_var(nbit_typ, div)));
                    BM_BINARY(op, less, BinaryOp::less, read_bytes, consumed_bytes);
                    op(AbstractOp::IF, [&](Code& m) {
                        m.ref(less);
                    });
                    {
                        BM_ERROR_WRAP(i_, error, (new_var(nbit_typ, read_bytes)));
                        BM_BINARY(op, less2, BinaryOp::less, i_, consumed_bytes);
                        op(AbstractOp::LOOP_CONDITION, [&](Code& m) {
                            m.ref(less2);
                        });
                        {
                            BM_INDEX(op, array_index, tmp_array, i_);
                            BM_DECODE_INT(op, array_index, endian, 8, code.ref().value());
                            BM_ERROR_WRAP(u8_typ, error, (get_nbit_typ(8, false)));
                            BM_ERROR_WRAP(u8_typ_storage, error, (m.get_storage(u8_typ)));
                            BM_IMMEDIATE(op, bit_mask, 0xff);
                            auto err = add_endian_specific(
                                m, endian, op,
                                [&]() {
                                    BM_BINARY(op, mul, BinaryOp::mul, i_, imm8);
                                    return assign_to_target(array_index, bit_mask, mul, u8_typ, u8_typ_storage);
                                },
                                [&]() {
                                    BM_IMMEDIATE(op, bit_size_div_8_minus_1, bit_size / 8 - 1);
                                    BM_BINARY(op, sub, BinaryOp::sub, bit_size_div_8_minus_1, i_);
                                    BM_BINARY(op, mul, BinaryOp::mul, sub, imm8);
                                    return assign_to_target(array_index, bit_mask, mul, u8_typ, u8_typ_storage);
                                });
                            BM_REF(op, AbstractOp::INC, i_);
                        }
                        BM_OP(op, AbstractOp::END_LOOP);
                        BM_ASSIGN(op, assign, read_bytes, consumed_bytes, null_varint, nullptr);
                    }
                    BM_OP(op, AbstractOp::END_IF);
                }

                // mask = ((1 << dec_bit_size) - 1)
                // on little endian:
                //   // get from lsb
                //  ref = src_type((target >> counter) & mask)
                //  counter += dec_bit_size
                // on big endian:
                //   // get from msb
                //  counter += dec_bit_size
                //  ref = src_type((target >> (nbit - counter)) & mask)
                // on native endian, platform dependent
                // on dynamic endian, dynamic variable dependent
                auto assign_to_ref = [&](Varint shift_index) {
                    BM_BINARY(op, shift, BinaryOp::right_logical_shift, target, shift_index);
                    BM_IMMEDIATE(op, mask, (safe_left_shift(1, dec_bit_size) - 1));
                    BM_BINARY(op, and_, BinaryOp::bit_and, shift, mask);
                    auto target_type_storage = m.get_storage(target_type);
                    if (!target_type_storage) {
                        return target_type_storage.error();
                    }
                    auto casted = add_assign_cast(m, op, &src_type_storage, &*target_type_storage, and_);
                    if (!casted) {
                        return casted.error();
                    }
                    BM_ASSIGN(op, assign, code.ref().value(), *casted ? **casted : and_, null_varint, nullptr);
                    return none;
                };
                auto err = add_endian_specific(
                    m, dec_endian, op,
                    [&]() {
                        BM_ERROR_WRAP_ERROR(error, (assign_to_ref(counter)));
                        return add_counter(dec_bit_size);
                    },
                    [&]() {
                        BM_ERROR_WRAP_ERROR(error, (add_counter(dec_bit_size)));
                        BM_IMMEDIATE(op, bit_size_, bit_size);
                        BM_BINARY(op, sub, BinaryOp::sub, bit_size_, counter);
                        return assign_to_ref(sub);
                    });
                if (err) {
                    return err;
                }
                op(AbstractOp::END_FALLBACK, [&](Code& m) {});
            }
        }
        m.code.insert(m.code.end(), new_code.begin(), new_code.end());
        return none;
    }

}  // namespace rebgn