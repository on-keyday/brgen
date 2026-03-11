/*license*/

#include "internal.hpp"
#include <core/ast/tool/eval.h>
#include "macro.hpp"

namespace rebgn {
    Varint get_field_ref(Module& m, ast::Field* field) {
        if (!field) {
            return *varint(null_id);
        }
        return *m.lookup_ident(field->ident);
    }

    expected<Varint> get_alignment_requirement(Module& m, ast::ArrayType* arr, ast::Field* field, bool on_encode) {
        auto ident = m.new_node_id(arr->length);
        if (!ident) {
            return ident;
        }
        m.op(on_encode ? AbstractOp::OUTPUT_BYTE_OFFSET : AbstractOp::INPUT_BYTE_OFFSET, [&](Code& c) {
            c.ident(*ident);
        });
        auto imm_alignment = immediate(m, *field->arguments->alignment_value / 8);
        if (!imm_alignment) {
            return imm_alignment;
        }

        // size % alignment
        BM_BINARY_UNEXPECTED(m.op, mod, BinaryOp::mod, *ident, *imm_alignment);

        // alignment - (size % alignment)
        BM_BINARY_UNEXPECTED(m.op, cmp, BinaryOp::sub, *imm_alignment, mod);

        // (alignment - (size % alignment)) % alignment
        BM_BINARY_UNEXPECTED(m.op, req_size, BinaryOp::mod, cmp, *imm_alignment);

        return define_int_tmp_var(m, req_size, ast::ConstantLevel::variable);
    }

    Error encode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref, std::shared_ptr<ast::Type> mapped_type, ast::Field* field, bool should_init_recursive) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            BM_GET_ENDIAN(endian, int_ty->endian, int_ty->is_signed);
            BM_ENCODE_INT(m.op, base_ref, endian, *int_ty->bit_size, (get_field_ref(m, field)));
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            BM_DEFINE_STORAGE(from, m, typ);
            BM_DEFINE_STORAGE(to, m, (std::make_shared<ast::IntType>(float_ty->loc, *float_ty->bit_size, ast::Endian::unspec, false)));
            BM_CAST(m.op, new_id, to, from, base_ref, CastType::FLOAT_TO_INT_BIT);
            BM_GET_ENDIAN(endian, float_ty->endian, false);
            BM_ENCODE_INT(m.op, new_id, endian, *float_ty->bit_size, (get_field_ref(m, field)));
            return none;
        }
        if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
            BM_ERROR_WRAP(str_ref, error, (static_str(m, str_ty->strong_ref)));
            auto max_len = immediate(m, *str_ty->bit_size / 8);
            if (!max_len) {
                return max_len.error();
            }
            return counter_loop(m, *max_len, [&](Varint counter) {
                BM_INDEX(m.op, index, str_ref, counter);
                BM_GET_ENDIAN(endian, Endian::unspec, false);
                BM_ENCODE_INT(m.op, index, endian, 8, (get_field_ref(m, field)));
                return none;
            });
        }
        if (auto arr = ast::as<ast::ArrayType>(typ)) {
            auto len = arr->length_value;
            auto elem_is_int = ast::as<ast::IntType>(arr->element_type);
            if (len) {
                auto imm = immediate(m, *len);
                if (!imm) {
                    return imm.error();
                }
                if (elem_is_int) {
                    BM_GET_ENDIAN(endian, elem_is_int->endian, elem_is_int->is_signed);
                    BM_ENCODE_INT_VEC_FIXED(m.op, base_ref, *imm, endian, *elem_is_int->bit_size, (get_field_ref(m, field)), *len);
                    return none;
                }
                return counter_loop(m, *imm, [&](Varint i) {
                    BM_INDEX(m.op, index, base_ref, i);
                    return encode_type(m, arr->element_type, index, mapped_type, field, should_init_recursive);
                });
            }
            if (!arr->length) {
                return error("Array length is not specified");
            }
            BM_NEW_NODE_ID(len_, error, arr->length);
            m.op(AbstractOp::ARRAY_SIZE, [&](Code& c) {
                c.ident(len_);
                c.ref(base_ref);
            });
            if (ast::is_any_range(arr->length)) {
                if (is_alignment_vector(field)) {
                    auto req_size = get_alignment_requirement(m, arr, field, true);
                    if (!req_size) {
                        return req_size.error();
                    }
                    BM_GET_ENDIAN(endian, Endian::unspec, false);
                    auto array_size = *field->arguments->alignment_value / 8 - 1;
                    BM_ENCODE_INT_VEC_FIXED(m.op, base_ref, *req_size, endian, 8, (get_field_ref(m, field)), array_size);
                    return none;
                }
            }
            else {
                auto len_init = get_expr(m, arr->length);
                if (!len_init) {
                    return len_init.error();
                }
                auto expr_type = arr->length->expr_type;
                if (auto u = ast::as<ast::UnionType>(expr_type)) {
                    if (!u->common_type) {
                        return error("Union type must have common type");
                    }
                    expr_type = u->common_type;
                }
                auto s = define_storage(m, expr_type);
                if (!s) {
                    return s.error();
                }
                auto expected_len = define_typed_tmp_var(m, *len_init, *s, ast::ConstantLevel::immutable_variable);
                if (!expected_len) {
                    return expected_len.error();
                }
                // add length check
                m.op(AbstractOp::LENGTH_CHECK, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*expected_len);
                    c.belong(get_field_ref(m, field));
                });
            }
            if (elem_is_int) {
                BM_GET_ENDIAN(endian, elem_is_int->endian, elem_is_int->is_signed);
                m.op(AbstractOp::ENCODE_INT_VECTOR, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(len_);
                    c.endian(endian);
                    c.bit_size(*varint(*elem_is_int->bit_size));
                    c.belong(get_field_ref(m, field));
                });
                return none;
            }
            return counter_loop(m, len_, [&](Varint counter) {
                BM_INDEX(m.op, index, base_ref, counter);
                auto err = encode_type(m, arr->element_type, index, mapped_type, field, false);
                if (err) {
                    return err;
                }
                return none;
            });
        }
        if (auto s = ast::as<ast::StructType>(typ)) {
            auto member = s->base.lock();
            if (auto me = ast::as<ast::Format>(member)) {  // only format can encode
                auto ident = m.lookup_ident(me->ident);
                if (!ident) {
                    return ident.error();
                }
                Varint bit_size_plus_1;
                if (me->body->struct_type->bit_size) {
                    auto s = varint(*me->body->struct_type->bit_size + 1);
                    if (!s) {
                        return s.error();
                    }
                    bit_size_plus_1 = s.value();
                }
                if (should_init_recursive && me->body->struct_type->recursive) {
                    m.op(AbstractOp::CHECK_RECURSIVE_STRUCT, [&](Code& c) {
                        c.left_ref(*ident);
                        c.right_ref(base_ref);
                    });
                }
                m.op(AbstractOp::CALL_ENCODE, [&](Code& c) {
                    c.left_ref(*ident);  // this is temporary and will rewrite to DEFINE_FUNCTION later
                    c.right_ref(base_ref);
                    c.bit_size_plus(bit_size_plus_1);
                });
                return none;
            }
            return error("unknown struct type: {}", node_type_to_string(member->node_type));
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("invalid enum type(maybe bug)");
            }
            auto base_type = base_enum->base_type;
            if (mapped_type) {
                base_type = mapped_type;
                mapped_type = nullptr;
            }
            if (!base_type) {
                return error("abstract enum {} in encode", base_enum->ident->ident);
            }
            BM_LOOKUP_IDENT(ident, m, base_enum->ident);
            BM_DEFINE_STORAGE(to, m, base_type);
            BM_DEFINE_STORAGE(from, m, typ);
            BM_CAST(m.op, casted, to, from, base_ref, CastType::ENUM_TO_INT);
            auto err = encode_type(m, base_type, casted, mapped_type, field, should_init_recursive);
            if (err) {
                return err;
            }
            return none;
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return encode_type(m, base_type, base_ref, mapped_type, field, should_init_recursive);
        }
        return error("unsupported type on encode type: {}", node_type_to_string(typ->node_type));
    }

    Error decode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref, std::shared_ptr<ast::Type> mapped_type, ast::Field* field, bool should_init_recursive) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            BM_GET_ENDIAN(endian, int_ty->endian, int_ty->is_signed);
            BM_DECODE_INT(m.op, base_ref, endian, *int_ty->bit_size, (get_field_ref(m, field)));
            return none;
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            BM_DEFINE_STORAGE(from, m, (std::make_shared<ast::IntType>(float_ty->loc, *float_ty->bit_size, ast::Endian::unspec, false)));
            BM_DEFINE_STORAGE(to, m, typ);
            BM_NEW_OBJECT(m.op, new_id, from);
            BM_ERROR_WRAP(tmp_var, error, (define_typed_tmp_var(m, new_id, from, ast::ConstantLevel::variable)));
            BM_GET_ENDIAN(endian, float_ty->endian, false);
            BM_DECODE_INT(m.op, tmp_var, endian, *float_ty->bit_size, (get_field_ref(m, field)));
            BM_CAST(m.op, next_id, to, from, tmp_var, CastType::INT_TO_FLOAT_BIT);
            return do_assign(m, nullptr, nullptr, base_ref, next_id);
        }
        if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
            auto str_ref = static_str(m, str_ty->strong_ref);
            auto max_len = immediate(m, *str_ty->bit_size / 8);
            if (!max_len) {
                return max_len.error();
            }
            return counter_loop(m, *max_len, [&](Varint counter) {
                auto int_typ = std::make_shared<ast::IntType>(str_ty->loc, 8, ast::Endian::unspec, false);
                BM_DEFINE_STORAGE(s, m, int_typ);
                BM_NEW_OBJECT(m.op, tmp, s);
                BM_ERROR_WRAP(tmp_var, error, define_typed_tmp_var(m, tmp, s, ast::ConstantLevel::variable));
                BM_GET_ENDIAN(endian, Endian::unspec, false);
                BM_DECODE_INT(m.op, tmp_var, endian, 8, (get_field_ref(m, field)));
                BM_INDEX(m.op, index, *str_ref, counter);
                BM_BINARY(m.op, cmp, BinaryOp::equal, index, tmp_var);
                m.op(AbstractOp::ASSERT, [&](Code& c) {
                    c.ref(cmp);
                    c.belong(m.get_function());
                });
                return none;
            });
        }
        if (auto arr = ast::as<ast::ArrayType>(typ)) {
            auto elem_is_int = ast::as<ast::IntType>(arr->element_type);
            auto len = arr->length_value;
            if (len) {
                auto imm = immediate(m, *len);
                if (!imm) {
                    return imm.error();
                }
                if (elem_is_int) {
                    BM_GET_ENDIAN(endian, elem_is_int->endian, elem_is_int->is_signed);
                    BM_DECODE_INT_VEC_FIXED(m.op, base_ref, *imm, endian, *elem_is_int->bit_size, (get_field_ref(m, field)), *len);
                    return none;
                }
                m.op(AbstractOp::RESERVE_SIZE, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*imm);
                    c.reserve_type(ReserveType::STATIC);
                });
                return counter_loop(m, *imm, [&](Varint i) {
                    BM_INDEX(m.op, index, base_ref, i);
                    return decode_type(m, arr->element_type, index, mapped_type, field, should_init_recursive);
                });
            }
            auto undelying_decoder = [&] {
                BM_DEFINE_STORAGE(s, m, arr->element_type);
                BM_NEW_OBJECT(m.op, new_obj, s);
                auto tmp_var = define_typed_tmp_var(m, new_obj, s, ast::ConstantLevel::variable);
                if (!tmp_var) {
                    return tmp_var.error();
                }
                auto err = decode_type(m, arr->element_type, *tmp_var, mapped_type, field, false);
                if (err) {
                    return err;
                }
                auto prev_ = m.prev_assign(tmp_var->value());
                m.op(AbstractOp::APPEND, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*varint(prev_));
                });
                return none;
            };
            if (ast::is_any_range(arr->length)) {
                if (field) {
                    if (is_alignment_vector(field)) {
                        auto req_size = get_alignment_requirement(m, arr, field, false);
                        if (!req_size) {
                            return req_size.error();
                        }
                        BM_GET_ENDIAN(endian, Endian::unspec, false);

                        BM_DECODE_INT_VEC_FIXED(m.op, base_ref, *req_size, endian, 8, (get_field_ref(m, field)), *field->arguments->alignment_value / 8 - 1);
                        return none;
                    }
                    else if (field->follow == ast::Follow::end ||
                             (field->arguments && field->arguments->sub_byte_length)  // this means that the field is a sub-byte field
                    ) {
                        if (elem_is_int) {
                            auto endian = m.get_endian(Endian(elem_is_int->endian), elem_is_int->is_signed);
                            if (!endian) {
                                return endian.error();
                            }
                            m.op(AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF, [&](Code& c) {
                                c.ref(base_ref);
                                c.endian(*endian);
                                c.bit_size(*varint(*elem_is_int->bit_size));
                                c.belong(get_field_ref(m, field));
                            });
                            return none;
                        }
                        BM_NEW_ID(new_id, error, nullptr);
                        m.op(AbstractOp::CAN_READ, [&](Code& c) {
                            c.ident(new_id);
                            c.belong(get_field_ref(m, field));
                        });
                        return conditional_loop(m, new_id, undelying_decoder);
                    }
                    else if (field->eventual_follow == ast::Follow::fixed) {
                        auto tail = field->belong_struct.lock()->fixed_tail_size / 8;
                        auto imm = immediate(m, tail);
                        if (!imm) {
                            return imm.error();
                        }
                        BM_NEW_ID(next_id, error, nullptr);
                        m.op(AbstractOp::REMAIN_BYTES, [&](Code& c) {
                            c.ident(next_id);
                        });
                        if (elem_is_int) {
                            // remain_bytes = REMAIN_BYTES - tail
                            // assert remain_bytes % elem_size == 0
                            // decode_int_vector(read_bytes / elem_size)
                            BM_BINARY(m.op, new_id, BinaryOp::sub, next_id, *imm);
                            auto elem_size = immediate(m, *elem_is_int->bit_size / futils::bit_per_byte);
                            if (!elem_size) {
                                return elem_size.error();
                            }
                            auto imm0 = immediate(m, 0);
                            if (!imm0) {
                                return imm0.error();
                            }
                            BM_BINARY(m.op, mod, BinaryOp::mod, new_id, *elem_size);
                            BM_BINARY(m.op, assert_expr, BinaryOp::equal, mod, *imm0);
                            m.op(AbstractOp::ASSERT, [&](Code& c) {
                                c.ref(assert_expr);
                                c.belong(m.get_function());
                            });
                            BM_GET_ENDIAN(endian, elem_is_int->endian, elem_is_int->is_signed);
                            m.op(AbstractOp::DECODE_INT_VECTOR, [&](Code& c) {
                                c.left_ref(base_ref);
                                c.right_ref(new_id);
                                c.endian(endian);
                                c.bit_size(*varint(*elem_is_int->bit_size));
                                c.belong(get_field_ref(m, field));
                            });
                            return none;
                        }
                        BM_BINARY(m.op, new_id, BinaryOp::grater, next_id, *imm);
                        return conditional_loop(m, new_id, undelying_decoder);
                    }
                    else if (field->follow == ast::Follow::constant) {
                        auto next = field->next.lock();
                        if (!next) {
                            return error("Invalid next field");
                        }
                        auto str = ast::cast_to<ast::StrLiteralType>(next->field_type);
                        auto str_ref = static_str(m, str->strong_ref);
                        auto imm = immediate(m, *str->bit_size / futils::bit_per_byte);
                        if (!imm) {
                            return imm.error();
                        }

                        Storages s;
                        s.length.value(2);
                        s.storages.push_back(Storage{.type = StorageType::ARRAY});
                        s.storages.back().size(*varint(*str->bit_size / futils::bit_per_byte));
                        s.storages.push_back(Storage{.type = StorageType::UINT});
                        s.storages.back().size(*varint(8));
                        auto ref = m.get_storage_ref(s, &next->loc);
                        if (!ref) {
                            return ref.error();
                        }
                        BM_NEW_OBJECT(m.op, new_id, *ref);
                        auto temporary_read_holder = define_typed_tmp_var(m, new_id, *ref, ast::ConstantLevel::variable);
                        if (!temporary_read_holder) {
                            return temporary_read_holder.error();
                        }
                        m.op(AbstractOp::LOOP_INFINITE);
                        {
                            auto endian = m.get_endian(Endian::unspec, false);
                            if (!endian) {
                                return endian.error();
                            }
                            m.op(AbstractOp::PEEK_INT_VECTOR, [&](Code& c) {
                                c.left_ref(*temporary_read_holder);
                                c.right_ref(*imm);
                                c.endian(*endian);
                                c.bit_size(*varint(8));
                                c.belong(get_field_ref(m, field));
                            });
                            Storages isOkFlag;
                            isOkFlag.length.value(1);
                            isOkFlag.storages.push_back(Storage{.type = StorageType::BOOL});
                            BM_GET_STORAGE_REF_WITH_LOC(gen, error, isOkFlag, &next->loc);
                            BM_NEW_OBJECT(m.op, flagObj, gen);
                            BM_ERROR_WRAP(isOK, error, (define_bool_tmp_var(m, flagObj, ast::ConstantLevel::variable)));
                            auto immTrue = immediate_bool(m, true);
                            if (!immTrue) {
                                return immTrue.error();
                            }
                            auto immFalse = immediate_bool(m, false);
                            if (!immFalse) {
                                return immFalse.error();
                            }
                            auto err = do_assign(m, nullptr, nullptr, isOK, *immTrue);
                            if (err) {
                                return err;
                            }
                            err = counter_loop(m, *imm, [&](Varint i) {
                                BM_BEGIN_COND_BLOCK(m.op, m.code, cond_block, nullptr);
                                BM_INDEX(m.op, index_str, *str_ref, i);
                                BM_INDEX(m.op, index_peek, *temporary_read_holder, i);
                                BM_BINARY(m.op, cmp, BinaryOp::not_equal, index_str, index_peek);
                                BM_END_COND_BLOCK(m.op, m.code, cond_block, cmp);
                                m.init_phi_stack(cmp.value());
                                BM_REF(m.op, AbstractOp::IF, cond_block);
                                err = do_assign(m, nullptr, nullptr, isOK, *immFalse);
                                m.op(AbstractOp::BREAK);
                                m.op(AbstractOp::END_IF);
                                return insert_phi(m, m.end_phi_stack());
                            });
                            if (err) {
                                return err;
                            }
                            BM_REF(m.op, AbstractOp::IF, isOK);
                            m.op(AbstractOp::BREAK);
                            m.op(AbstractOp::END_IF);
                            err = undelying_decoder();
                            if (err) {
                                return err;
                            }
                        }
                        m.op(AbstractOp::END_LOOP);
                        return none;
                    }
                    else {
                        return error("Invalid follow type");
                    }
                }
                else {
                    return error("Invalid field");
                }
            }
            else {
                BM_GET_EXPR(id, m, arr->length);
                auto expr_type = arr->length->expr_type;
                if (auto u = ast::as<ast::UnionType>(expr_type)) {
                    if (!u->common_type) {
                        return error("Union type must have common type");
                    }
                    expr_type = u->common_type;
                }
                BM_DEFINE_STORAGE(s, m, expr_type);
                auto len_ident = define_typed_tmp_var(m, id, s, ast::ConstantLevel::immutable_variable);
                if (!len_ident) {
                    return len_ident.error();
                }
                if (elem_is_int) {
                    BM_GET_ENDIAN(endian, elem_is_int->endian, elem_is_int->is_signed);
                    m.op(AbstractOp::DECODE_INT_VECTOR, [&](Code& c) {
                        c.left_ref(base_ref);
                        c.right_ref(*len_ident);
                        c.endian(endian);
                        c.bit_size(*varint(*elem_is_int->bit_size));
                        c.belong(get_field_ref(m, field));
                    });
                    return none;
                }
                m.op(AbstractOp::RESERVE_SIZE, [&](Code& c) {
                    c.left_ref(base_ref);
                    c.right_ref(*len_ident);
                    c.reserve_type(ReserveType::DYNAMIC);
                });
                return counter_loop(m, *len_ident, [&](Varint) {
                    return undelying_decoder();
                });
            }
        }
        if (auto s = ast::as<ast::StructType>(typ)) {
            auto member = s->base.lock();
            if (auto me = ast::as<ast::Format>(member)) {  // only format can decode
                auto ident = m.lookup_ident(me->ident);
                if (!ident) {
                    return ident.error();
                }
                Varint bit_size_plus_1;
                if (me->body->struct_type->bit_size) {
                    auto s = varint(*me->body->struct_type->bit_size + 1);
                    if (!s) {
                        return s.error();
                    }
                    bit_size_plus_1 = s.value();
                }
                if (should_init_recursive && me->body->struct_type->recursive) {
                    m.op(AbstractOp::INIT_RECURSIVE_STRUCT, [&](Code& c) {
                        c.left_ref(*ident);
                        c.right_ref(base_ref);
                    });
                }
                m.op(AbstractOp::CALL_DECODE, [&](Code& c) {
                    c.left_ref(*ident);  // this is temporary and will rewrite to DEFINE_FUNCTION later
                    c.right_ref(base_ref);
                    c.bit_size_plus(bit_size_plus_1);
                });
                return none;
            }
            return error("unknown struct type: {}", node_type_to_string(member->node_type));
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("invalid enum type(maybe bug)");
            }
            auto base_type = base_enum->base_type;
            if (mapped_type) {
                base_type = mapped_type;
                mapped_type = nullptr;
            }
            if (!base_type) {
                return error("abstract enum {} in decode", base_enum->ident->ident);
            }
            BM_LOOKUP_IDENT(ident, m, base_enum->ident);
            BM_DEFINE_STORAGE(from, m, base_type);
            BM_NEW_OBJECT(m.op, storage, from);
            auto tmp_var = define_typed_tmp_var(m, storage, from, ast::ConstantLevel::variable);
            if (!tmp_var) {
                return tmp_var.error();
            }
            auto err = decode_type(m, base_type, *tmp_var, mapped_type, field, should_init_recursive);
            if (err) {
                return err;
            }

            BM_DEFINE_STORAGE(to, m, ast::cast_to<ast::EnumType>(typ));
            BM_CAST(m.op, casted, to, from, *tmp_var, CastType::INT_TO_ENUM);
            return do_assign(m, nullptr, nullptr, base_ref, casted);
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return decode_type(m, base_type, base_ref, mapped_type, field, should_init_recursive);
        }
        return error("unsupported type on decode type: {}", node_type_to_string(typ->node_type));
    }

    Error do_field_argument_assert(Module& m, Varint ident, std::shared_ptr<ast::Field>& node) {
        if (node->arguments && node->arguments->arguments.size()) {
            if (size_t(node->arguments->argument_mapping) & size_t(ast::FieldArgumentMapping::direct)) {
                std::optional<Varint> prev;
                for (auto& arg : node->arguments->arguments) {
                    BM_GET_EXPR(val, m, arg);
                    BM_BINARY(m.op, new_id, BinaryOp::equal, ident, val);
                    if (prev) {
                        BM_BINARY(m.op, or_id, BinaryOp::logical_or, new_id, *prev);
                        prev = or_id;
                    }
                    else {
                        prev = new_id;
                    }
                }
                m.op(AbstractOp::ASSERT, [&](Code& c) {
                    c.ref(*prev);
                    c.belong(m.get_function());
                });
            }
        }
        return none;
    }

    template <>
    Error encode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        if (node->is_state_variable) {
            return none;
        }
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        auto err = do_field_argument_assert(m, *ident, node);
        if (err) {
            return err;
        }
        std::optional<Varint> seek_pos_holder;
        if (node->arguments && node->arguments->sub_byte_length) {
            if (node->arguments->sub_byte_begin) {
                auto offset = get_expr(m, node->arguments->sub_byte_begin);
                if (!offset) {
                    return offset.error();
                }
                auto save_current_pos = m.new_id(nullptr);
                if (!save_current_pos) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::OUTPUT_BYTE_OFFSET, [&](Code& c) {
                    c.ident(*save_current_pos);
                });
                auto tmpvar = define_int_tmp_var(m, *save_current_pos, ast::ConstantLevel::immutable_variable);
                if (!tmpvar) {
                    return tmpvar.error();
                }
                seek_pos_holder = *tmpvar;
                m.op(AbstractOp::SEEK_ENCODER, [&](Code& c) {
                    c.ref(*offset);
                    c.belong(*ident);
                });
            }
            if (ast::is_any_range(node->arguments->sub_byte_length)) {
                if (!node->arguments->sub_byte_begin) {
                    return error("until eof subrange is not supported without begin");
                }
            }
            else {
                auto len = get_expr(m, node->arguments->sub_byte_length);
                if (!len) {
                    return len.error();
                }
                m.op(AbstractOp::BEGIN_ENCODE_SUB_RANGE, [&](Code& c) {
                    c.sub_range_type(SubRangeType::byte_len);
                    c.ref(*len);
                    c.belong(*ident);
                });
            }
        }
        if (node->arguments && node->arguments->type_map) {
            err = encode_type(m, node->field_type, *ident, node->arguments->type_map->type_literal, node.get(), true);
        }
        else {
            err = encode_type(m, node->field_type, *ident, nullptr, node.get(), true);
        }
        if (err) {
            return err;
        }
        if (node->arguments && node->arguments->sub_byte_length &&
            !ast::is_any_range(node->arguments->sub_byte_length)) {
            m.op(AbstractOp::END_ENCODE_SUB_RANGE);
        }
        if (seek_pos_holder) {
            m.op(AbstractOp::SEEK_ENCODER, [&](Code& c) {
                c.ref(*seek_pos_holder);
                c.belong(*ident);
            });
        }
        return none;
    }

    template <>
    Error decode<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        if (node->is_state_variable) {
            return none;
        }
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        std::optional<Varint> seek_pos_holder;
        if (node->arguments && node->arguments->sub_byte_length) {
            if (node->arguments->sub_byte_begin) {
                auto offset = get_expr(m, node->arguments->sub_byte_begin);
                if (!offset) {
                    return offset.error();
                }
                auto save_current_pos = m.new_id(nullptr);
                if (!save_current_pos) {
                    return error("Failed to generate new id");
                }
                m.op(AbstractOp::INPUT_BYTE_OFFSET, [&](Code& c) {
                    c.ident(*save_current_pos);
                });
                auto tmpvar = define_int_tmp_var(m, *save_current_pos, ast::ConstantLevel::immutable_variable);
                if (!tmpvar) {
                    return tmpvar.error();
                }
                seek_pos_holder = *tmpvar;
                m.op(AbstractOp::SEEK_DECODER, [&](Code& c) {
                    c.ref(*offset);
                    c.belong(*ident);
                });
            }
            if (ast::is_any_range(node->arguments->sub_byte_length)) {
                if (!node->arguments->sub_byte_begin) {
                    return error("until eof subrange is not supported without begin");
                }
            }
            else {
                auto len = get_expr(m, node->arguments->sub_byte_length);
                if (!len) {
                    return len.error();
                }
                m.op(AbstractOp::BEGIN_DECODE_SUB_RANGE, [&](Code& c) {
                    c.sub_range_type(SubRangeType::byte_len);
                    c.ref(*len);
                    c.belong(*ident);
                });
            }
        }
        Error err;
        if (node->arguments && node->arguments->type_map) {
            err = decode_type(m, node->field_type, *ident, node->arguments->type_map->type_literal, node.get(), true);
        }
        else {
            err = decode_type(m, node->field_type, *ident, nullptr, node.get(), true);
        }
        if (err) {
            return err;
        }
        if (node->arguments && node->arguments->sub_byte_length &&
            !ast::is_any_range(node->arguments->sub_byte_length)) {
            m.op(AbstractOp::END_DECODE_SUB_RANGE);
        }
        if (seek_pos_holder) {
            m.op(AbstractOp::SEEK_DECODER, [&](Code& c) {
                c.ref(*seek_pos_holder);
                c.belong(*ident);
            });
        }
        err = do_field_argument_assert(m, *ident, node);
        if (err) {
            return err;
        }
        return err;
    }

    Endian get_type_endian(const std::shared_ptr<ast::Type>& typ) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            return Endian(int_ty->endian);
        }
        if (auto float_ty = ast::as<ast::FloatType>(typ)) {
            return Endian(float_ty->endian);
        }
        if (auto enum_ty = ast::as<ast::EnumType>(typ)) {
            return get_type_endian(enum_ty->base.lock()->base_type);
        }
        return Endian::unspec;
    }

    template <>
    Error encode<ast::Format>(Module& m, std::shared_ptr<ast::Format>& node) {
        auto fmt_ident = m.lookup_ident(node->ident);
        if (!fmt_ident) {
            return fmt_ident.error();
        }
        auto fn = node->encode_fn.lock();
        if (fn) {
            auto ident = m.lookup_ident(fn->ident);
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::DEFINE_ENCODER, [&](Code& c) {
                c.left_ref(*fmt_ident);
                c.right_ref(*ident);
            });
            return none;
        }
        auto temporary_ident = std::make_shared<ast::Ident>(node->loc, "encode");
        temporary_ident->base = node;  // for lookup
        auto new_id = m.lookup_ident(temporary_ident);
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*new_id);
            c.belong(*fmt_ident);
            c.func_type(FunctionType::ENCODE);
        });
        auto typ = m.get_storage_ref(Storages{
                                         .length = varint(1).value(),
                                         .storages = {
                                             Storage{.type = StorageType::CODER_RETURN},
                                         },
                                     },
                                     &node->loc);
        if (!typ) {
            return typ.error();
        }
        m.op(AbstractOp::RETURN_TYPE, [&](Code& c) {
            c.type(*typ);
        });
        m.on_encode_fn = true;
        m.init_phi_stack(0);  // make it temporary
        auto f = m.enter_function(*new_id);
        auto err = foreach_node(m, node->body->elements, [&](auto& n) {
            if (auto found = m.bit_field_begin.find(n);
                found != m.bit_field_begin.end()) {
                auto new_id = m.new_id(nullptr);
                if (!new_id) {
                    return error("Failed to generate new id");
                }
                auto typ = m.bit_field_variability[n];
                auto field = ast::as<ast::Field>(n);
                m.op(AbstractOp::BEGIN_ENCODE_PACKED_OPERATION, [&](Code& c) {
                    c.ident(*new_id);
                    c.belong(found->second);
                    c.packed_op_type(typ);
                    c.endian(*m.get_endian(get_type_endian(field ? field->field_type : nullptr), false));
                });
            }
            auto err = convert_node_encode(m, n);
            if (m.bit_field_end.contains(n)) {
                m.op(AbstractOp::END_ENCODE_PACKED_OPERATION);
            }
            return err;
        });
        if (err) {
            return err;
        }
        f.execute();
        m.op(AbstractOp::RET_SUCCESS, [&](Code& c) {
            c.belong(*new_id);
        });
        m.op(AbstractOp::END_FUNCTION);
        m.end_phi_stack();  // remove temporary
        m.op(AbstractOp::DEFINE_ENCODER, [&](Code& c) {
            c.left_ref(*fmt_ident);
            c.right_ref(*new_id);
        });
        return none;
    }

    template <>
    Error decode<ast::Format>(Module& m, std::shared_ptr<ast::Format>& node) {
        auto fmt_ident = m.lookup_ident(node->ident);
        if (!fmt_ident) {
            return fmt_ident.error();
        }
        auto fn = node->decode_fn.lock();
        if (fn) {
            auto ident = m.lookup_ident(fn->ident);
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::DEFINE_DECODER, [&](Code& c) {
                c.left_ref(*fmt_ident);
                c.right_ref(*ident);
            });
            return none;
        }
        auto temporary_ident = std::make_shared<ast::Ident>(node->loc, "decode");
        temporary_ident->base = node;  // for lookup
        auto new_id = m.lookup_ident(temporary_ident);
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*new_id);
            c.belong(fmt_ident.value());
            c.func_type(FunctionType::DECODE);
        });
        auto typ = m.get_storage_ref(Storages{
                                         .length = varint(1).value(),
                                         .storages = {
                                             Storage{.type = StorageType::CODER_RETURN},
                                         },
                                     },
                                     &node->loc);
        if (!typ) {
            return typ.error();
        }
        m.op(AbstractOp::RETURN_TYPE, [&](Code& c) {
            c.type(*typ);
        });
        m.on_encode_fn = false;
        m.init_phi_stack(0);  // make it temporary
        auto f = m.enter_function(*new_id);
        auto err = foreach_node(m, node->body->elements, [&](auto& n) {
            if (auto found = m.bit_field_begin.find(n);
                found != m.bit_field_begin.end()) {
                auto new_id = m.new_id(nullptr);
                if (!new_id) {
                    return error("Failed to generate new id");
                }
                auto typ = m.bit_field_variability[n];
                auto field = ast::as<ast::Field>(n);
                m.op(AbstractOp::BEGIN_DECODE_PACKED_OPERATION, [&](Code& c) {
                    c.ident(*new_id);
                    c.belong(found->second);
                    c.packed_op_type(typ);
                    c.endian(*m.get_endian(get_type_endian(field ? field->field_type : nullptr), false));
                });
            }
            auto err = convert_node_decode(m, n);
            if (m.bit_field_end.contains(n)) {
                m.op(AbstractOp::END_DECODE_PACKED_OPERATION);
            }
            return err;
        });
        if (err) {
            return err;
        }
        f.execute();
        m.op(AbstractOp::RET_SUCCESS, [&](Code& c) {
            c.belong(*new_id);
        });
        m.op(AbstractOp::END_FUNCTION);
        m.end_phi_stack();  // remove temporary
        m.op(AbstractOp::DEFINE_DECODER, [&](Code& c) {
            c.left_ref(*fmt_ident);
            c.right_ref(*new_id);
        });
        return none;
    }

    template <>
    Error encode<ast::Program>(Module& m, std::shared_ptr<ast::Program>& node) {
        return foreach_node(m, node->elements, [&](auto& n) {
            return convert_node_encode(m, n);
        });
    }

    template <>
    Error decode<ast::Program>(Module& m, std::shared_ptr<ast::Program>& node) {
        return foreach_node(m, node->elements, [&](auto& n) {
            return convert_node_decode(m, n);
        });
    }

    void add_switch_union(Module& m, std::shared_ptr<ast::StructType>& node) {
        auto ident = m.lookup_struct(node);
        if (ident.value() == 0) {
            return;
        }
        bool has_field = false;
        for (auto& field : node->fields) {
            if (ast::as<ast::Field>(field)) {
                has_field = true;
                break;
            }
        }
        if (!has_field) {
            return;  // omit check for empty struct
        }
        if (m.on_encode_fn) {
            m.op(AbstractOp::CHECK_UNION, [&](Code& c) {
                c.ref(ident);
                c.check_at(UnionCheckAt::ENCODER);
            });
        }
        else {
            m.op(AbstractOp::SWITCH_UNION, [&](Code& c) {
                c.ref(ident);
            });
        }
    }

    template <>
    Error encode<ast::Match>(Module& m, std::shared_ptr<ast::Match>& node) {
        return convert_match(m, node, [](Module& m, auto& n) {
            return convert_node_encode(m, n);
        });
    }

    template <>
    Error decode<ast::Match>(Module& m, std::shared_ptr<ast::Match>& node) {
        return convert_match(m, node, [](Module& m, auto& n) {
            return convert_node_decode(m, n);
        });
    }

    template <>
    Error encode<ast::If>(Module& m, std::shared_ptr<ast::If>& node) {
        return convert_if(m, node, [](Module& m, auto& n) {
            return convert_node_encode(m, n);
        });
    }

    template <>
    Error decode<ast::If>(Module& m, std::shared_ptr<ast::If>& node) {
        return convert_if(m, node, [](Module& m, auto& n) {
            return convert_node_decode(m, n);
        });
    }

    template <>
    Error encode<ast::Loop>(Module& m, std::shared_ptr<ast::Loop>& node) {
        return convert_loop(m, node, [](Module& m, auto& n) {
            return convert_node_encode(m, n);
        });
    }

    template <>
    Error decode<ast::Loop>(Module& m, std::shared_ptr<ast::Loop>& node) {
        return convert_loop(m, node, [](Module& m, auto& n) {
            return convert_node_decode(m, n);
        });
    }

    template <>
    Error encode<ast::SpecifyOrder>(Module& m, std::shared_ptr<ast::SpecifyOrder>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::SpecifyOrder>(Module& m, std::shared_ptr<ast::SpecifyOrder>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error encode<ast::Break>(Module& m, std::shared_ptr<ast::Break>& node) {
        m.op(AbstractOp::BREAK);
        return none;
    }

    template <>
    Error decode<ast::Break>(Module& m, std::shared_ptr<ast::Break>& node) {
        m.op(AbstractOp::BREAK);
        return none;
    }

    template <>
    Error encode<ast::Continue>(Module& m, std::shared_ptr<ast::Continue>& node) {
        m.op(AbstractOp::CONTINUE);
        return none;
    }

    template <>
    Error decode<ast::Continue>(Module& m, std::shared_ptr<ast::Continue>& node) {
        m.op(AbstractOp::CONTINUE);
        return none;
    }

    template <>
    Error encode<ast::Assert>(Module& m, std::shared_ptr<ast::Assert>& node) {
        return convert_node_definition(m, node);
    }

    template <>
    Error decode<ast::Assert>(Module& m, std::shared_ptr<ast::Assert>& node) {
        return convert_node_definition(m, node);
    }

    template <class T>
    concept has_encode = requires(Module& m, std::shared_ptr<T>& n) {
        encode<T>(m, n);
    };

    template <class T>
    concept has_decode = requires(Module& m, std::shared_ptr<T>& n) {
        decode<T>(m, n);
    };

    Error convert_node_encode(Module& m, const std::shared_ptr<ast::Node>& n) {
        Error err;
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            if constexpr (has_encode<T>) {
                err = encode<T>(m, node);
            }
            else if (std::is_base_of_v<ast::Expr, T>) {
                err = convert_node_definition(m, node);
            }
        });
        return err;
    }

    Error convert_node_decode(Module& m, const std::shared_ptr<ast::Node>& n) {
        Error err;
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            if constexpr (has_decode<T>) {
                err = decode<T>(m, node);
            }
            else if (std::is_base_of_v<ast::Expr, T>) {
                err = convert_node_definition(m, node);
            }
        });
        return err;
    }
}  // namespace rebgn
