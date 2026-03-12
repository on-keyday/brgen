/*license*/
#include <bmgen/internal.hpp>
#include <bmgen/fallback.hpp>
#include <bmgen/bit.hpp>
#include <algorithm>

namespace rebgn {
    expected<size_t> bit_size(const Storages& s) {
        if (s.storages.size() == 0) {
            return unexpect_error("Invalid storage size");
        }
        size_t index = 0;
        if (s.storages[index].type == StorageType::ENUM) {
            index++;
            if (s.storages.size() == index) {
                return unexpect_error("Invalid storage size");
            }
        }
        if (s.storages[index].type == StorageType::UINT || s.storages[index].type == StorageType::INT) {
            return s.storages[index].size()->value();
        }
        if (s.storages[index].type == StorageType::STRUCT_REF) {
            if (s.storages[index].size()->value() == 0) {
                return unexpect_error("Invalid storage size");
            }
            return s.storages[index].size()->value() - 1;
        }
        if (s.storages[index].type == StorageType::VARIANT) {
            std::uint64_t candidate = 0;
            for (index++; index < s.storages.size(); index++) {
                if (s.storages[index].type != StorageType::STRUCT_REF) {
                    return unexpect_error("Invalid storage type");
                }
                auto size = s.storages[index].size()->value();
                if (size == 0) {
                    return unexpect_error("Invalid storage size");
                }
                candidate = std::max(candidate, size - 1);
            }
            return candidate;
        }
        return unexpect_error("Invalid storage type");
    }

    Error derive_bit_field_accessor_functions(Module& m) {
        std::vector<Code> new_code;
        NEW_CODE_OP(new_code);
        std::optional<Varint> target;
        std::optional<StorageRef> bit_field_type;
        std::optional<Storages> detailed_field_type;
        size_t offset = 0;
        size_t max_offset = 0;
        std::optional<Varint> belongs;
        std::unordered_map<ObjectID, std::pair<Varint, Varint>> bit_fields_fn_map;

        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_BIT_FIELD) {
                target = c.ident();
                bit_field_type = c.type();
                belongs = c.belong();
                BM_ERROR_WRAP(detailed_type, error, m.get_storage(c.type().value()));
                detailed_field_type = detailed_type;
                auto n_bit_sum = bit_size(detailed_type);
                if (!n_bit_sum) {
                    return n_bit_sum.error();
                }
                max_offset = *n_bit_sum;
            }
            if (c.op == AbstractOp::END_BIT_FIELD) {
                target.reset();
                bit_field_type.reset();
                detailed_field_type.reset();
                belongs.reset();
                offset = 0;
            }
            if (target && c.op == AbstractOp::DEFINE_FIELD) {
                BM_ERROR_WRAP(detailed_type, error, m.get_storage(c.type().value()));
                BM_ERROR_WRAP(n_bit, error, bit_size(detailed_type));

                std::uint64_t max_mask = safe_left_shift(1, n_bit) - 1;

                auto base = m.ident_table_rev[c.ident().value().value()];
                auto temporary_getter_ident = std::make_shared<ast::Ident>(base->loc, base->ident);
                temporary_getter_ident->base = base->base;
                BM_ERROR_WRAP(getter_id, error, m.lookup_ident(temporary_getter_ident));
                auto temporary_setter_ident = std::make_shared<ast::Ident>(base->loc, base->ident);
                temporary_setter_ident->base = base->base;
                BM_ERROR_WRAP(setter_id, error, m.lookup_ident(temporary_setter_ident));
                auto shift_value = max_offset - offset - n_bit;
                BM_IMMEDIATE(op, shift_offset, shift_value);
                BM_IMMEDIATE(op, mask, max_mask);

                bit_fields_fn_map[c.ident()->value()] = {getter_id, setter_id};

                {
                    op(AbstractOp::DEFINE_FUNCTION, [&](Code& r) {
                        r.ident(getter_id);
                        r.belong(*belongs);
                        r.func_type(FunctionType::BIT_GETTER);
                    });

                    op(AbstractOp::RETURN_TYPE, [&](Code& r) {
                        r.type(c.type().value());
                    });

                    // target_type((bit_fields >> (n bit - offset)) & max(n bit))

                    BM_BINARY(op, shift, BinaryOp::right_logical_shift, *target, shift_offset);
                    BM_BINARY(op, and_, BinaryOp::bit_and, shift, mask);
                    auto cast_type = get_cast_type(detailed_type, *detailed_field_type);
                    BM_CAST(op, cast, c.type().value(), *bit_field_type, and_, cast_type);

                    op(AbstractOp::RET, [&](Code& c) {
                        c.ref(cast);
                        c.belong(getter_id);
                    });

                    BM_OP(op, AbstractOp::END_FUNCTION);
                }
                {
                    op(AbstractOp::DEFINE_FUNCTION, [&](Code& r) {
                        r.ident(setter_id);
                        r.belong(*belongs);
                        r.func_type(FunctionType::BIT_SETTER);
                    });

                    auto ret_type_ref = m.get_storage_ref(
                        Storages{
                            .length = *varint(1),
                            .storages = {Storage{.type = StorageType::PROPERTY_SETTER_RETURN}},
                        },
                        nullptr);
                    if (!ret_type_ref) {
                        return ret_type_ref.error();
                    }

                    op(AbstractOp::RETURN_TYPE, [&](Code& c) {
                        c.type(*ret_type_ref);
                    });

                    BM_NEW_ID(prop_input, error, nullptr);
                    op(AbstractOp::PROPERTY_INPUT_PARAMETER, [&](Code& r) {
                        r.ident(prop_input);
                        r.left_ref(c.ident().value());
                        r.right_ref(setter_id);
                        r.type(c.type().value());
                    });

                    auto n_bit_type = Storages{
                        .length = *varint(1),
                        .storages = {Storage{.type = StorageType::UINT}},
                    };
                    n_bit_type.storages[0].size(*varint(n_bit));
                    auto n_bit_type_ref = m.get_storage_ref(
                        n_bit_type,
                        nullptr);
                    if (!n_bit_type_ref) {
                        return n_bit_type_ref.error();
                    }
                    Varint casted;
                    if (n_bit_type_ref->ref.value() != c.type()->ref.value()) {
                        auto cast_type = get_cast_type(n_bit_type, detailed_type);
                        BM_CAST(op, cast, *n_bit_type_ref, c.type().value(), prop_input, cast_type);
                        casted = cast;
                    }
                    else {
                        casted = prop_input;
                    }

                    // assert casted <= max(n bit)
                    BM_BINARY(op, asset_expr, BinaryOp::less_or_eq, casted, mask);
                    op(AbstractOp::ASSERT, [&](Code& c) {
                        c.ref(asset_expr);
                        c.belong(setter_id);
                    });

                    auto casted_to_bit_field_type = get_cast_type(*detailed_field_type, n_bit_type);
                    BM_CAST(op, casted_to_bit_field, *bit_field_type, *n_bit_type_ref, casted, casted_to_bit_field_type);

                    // target = (target & ~(max(n bit) << offset)) | ((casted & max(n bit)) << offset)
                    auto mask_not = (~(max_mask << (max_offset - offset - n_bit))) & (safe_left_shift(1, max_offset) - 1);
                    BM_IMMEDIATE(op, mask_not_id, mask_not);
                    BM_BINARY(op, and_, BinaryOp::bit_and, *target, mask_not_id);
                    BM_BINARY(op, input_masked, BinaryOp::bit_and, casted_to_bit_field, mask);
                    BM_BINARY(op, shift, BinaryOp::left_logical_shift, input_masked, shift_offset);
                    BM_BINARY(op, or_, BinaryOp::bit_or, and_, shift);

                    BM_ASSIGN(op, assign, *target, or_, null_varint, nullptr);

                    op(AbstractOp::RET_PROPERTY_SETTER_OK, [&](Code& c) {
                        c.belong(setter_id);
                    });

                    BM_OP(op, AbstractOp::END_FUNCTION);
                }
                offset += n_bit;
            }
        }
        std::vector<Code> next_code;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_FIELD) {
                auto found = bit_fields_fn_map.find(c.ident()->value());
                if (found != bit_fields_fn_map.end()) {
                    Code getter;
                    Code setter;
                    getter.op = AbstractOp::DECLARE_FUNCTION;
                    setter.op = AbstractOp::DECLARE_FUNCTION;
                    getter.ref(found->second.first);
                    setter.ref(found->second.second);
                    next_code.push_back(std::move(c));
                    next_code.push_back(std::move(getter));
                    next_code.push_back(std::move(setter));
                }
                else {
                    next_code.push_back(std::move(c));
                }
            }
            else {
                next_code.push_back(std::move(c));
            }
        }
        next_code.insert_range(next_code.end(), std::move(new_code));
        m.code = std::move(next_code);
        return none;
    }
}  // namespace rebgn
