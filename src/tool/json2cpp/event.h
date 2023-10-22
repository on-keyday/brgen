/*license*/
#pragma once
#include "desc.h"

namespace json2cpp {
    struct DefineLength {
        std::string field_name;
        std::string length;
    };

    struct AssertFalse {
        std::string comment;
        std::string cond;
    };

    struct ReturnError {
        std::string comment;
    };

    struct ApplyBulkInt {
        std::vector<std::string> field_names;
    };

    struct ApplyVector {
        std::string field_name;
        std::string length_var;
    };

    struct AllocateVector {
        std::string field_name;
        std::string element_type;
        std::string length_var;
    };

    struct ApplyInt {
        std::string field_name;
    };

    struct BitField {
        std::string bit_size;
        std::string field_name;
    };

    struct ApplyBits {
        std::string base_name;
        std::string base_type;
        std::vector<BitField> field_names;
    };

    struct DefineBits {
        std::string var_name;
        std::string from;
    };

    struct SetBits {
        std::string base_field;
        std::string bit_field_index;
        std::string value;
    };

    struct IfBegin {
        std::string cond;
        bool elif = false;
    };

    struct ElseBegin {};

    struct BlockEnd {};

    struct Assign {
        std::string to;
        std::string from;
    };

    using Event = std::variant<
        ApplyInt, DefineLength, AssertFalse,
        ApplyBits, DefineBits, SetBits, ApplyBulkInt,
        AllocateVector, ApplyVector,
        IfBegin, ElseBegin, BlockEnd,
        Assign, ReturnError>;

    using Events = std::vector<Event>;

    enum class Method {
        encode,
        decode,
        move,
    };

    struct EventConverter {
        Config& config;
        Events events;

       private:
        std::string tmp_len_of(const std::string& name) {
            return "tmp_len_of_" + name;
        }

        result<void> add_bulk_array(ApplyBulkInt& bulk, const std::shared_ptr<Field>& field) {
            auto len = static_cast<IntArrayDesc*>(field->desc.get())->desc.length_eval->get<tool::EResultType::integer>();
            for (auto i = 0; i < len; i++) {
                bulk.field_names.push_back(field->name + "[" + brgen::nums(i) + "]");
            }
            return {};
        }

        void add_type_overflow_assert(const std::string& target, const std::shared_ptr<Field>& len_field) {
            assert(len_field->desc->type == DescType::int_);
            auto int_desc = static_cast<IntDesc*>(len_field->desc.get());
            if (auto bit = int_desc->bit_field.lock()) {
                events.push_back(AssertFalse{
                    .comment = "check overflow",
                    .cond = brgen::concat(target, " > ", len_field->name, "_max"),
                });
            }
            else {
                config.includes.emplace("<limits>");
                events.push_back(AssertFalse{
                    .comment = "check overflow",
                    .cond = brgen::concat(target, " > ", "(std::numeric_limits<", std::string(get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed)), ">::max)()"),
                });
            }
        }

        result<std::string> handle_encoder_int_length_related_std_vector(
            const std::shared_ptr<Field>& len_field, const std::shared_ptr<Field>& vec_field) {
            auto desc = static_cast<VectorDesc*>(vec_field->desc.get());
            tool::Stringer s;
            auto tmp_var = tmp_len_of(vec_field->name);
            auto length = vec_field->name + ".size()";
            s.tmp_var_map[0] = length;
            auto value_to_be_length_field = s.to_string(desc->resolved_expr);
            events.push_back(DefineLength{
                .field_name = tmp_var,
                .length = value_to_be_length_field,
            });
            add_type_overflow_assert(tmp_var, len_field);
            if (length != value_to_be_length_field) {
                s.ident_map[len_field->name] = tmp_var;
                value_to_be_length_field = s.to_string(desc->desc.length);
                events.push_back(AssertFalse{
                    .comment = "check truncated",
                    .cond = brgen::concat(length, " != ", value_to_be_length_field),
                });
            }
            auto int_desc = static_cast<IntDesc*>(len_field->desc.get());
            auto type = get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed);
            return brgen::concat(type, "(", tmp_var, ")");
        }

        result<std::string> handle_encoder_int_length_related_pointer(const std::shared_ptr<Field>& len_field, const std::shared_ptr<Field>& vec_field) {
            auto desc = static_cast<VectorDesc*>(vec_field->desc.get());
            tool::Stringer s;
            auto tmp = tmp_len_of(vec_field->name);
            auto base = brgen::concat("size_t(", len_field->name, ")");
            std::shared_ptr<ast::Expr> length_expr, check_length;

            if (config.asymmetric) {
                // use length field as length of vector
                // for example
                // format A:
                //    length :u32
                //    data :[length]u8
                // then converted to:
                // struct A {
                //    std::uint32_t length;
                //    uint8_t* data;
                //    bool render(binary::writer& w) {
                //        if (length > std::numeric_limits<std::uint32_t>::max) {
                //           return false;
                //        }
                //        w.write(length);
                //        w.write(data, length);
                //    }
                // };
                s.ident_map[len_field->name] = tmp;
                s.tmp_var_map[0] = base;
                length_expr = desc->resolved_expr;
                check_length = desc->desc.length;
            }
            else {
                s.tmp_var_map[0] = tmp;
                s.ident_map[len_field->name] = base;
                length_expr = desc->desc.length;
                check_length = desc->resolved_expr;
            }
            auto length = s.to_string(length_expr);
            events.push_back(DefineLength{
                .field_name = tmp,
                .length = length,
            });
            add_type_overflow_assert(tmp, len_field);
            if (length != base) {
                length = s.to_string(check_length);
                events.push_back(AssertFalse{
                    .comment = "check truncated",
                    .cond = brgen::concat(base, " != ", length),
                });
            }
            if (config.asymmetric) {
                auto int_desc = static_cast<IntDesc*>(len_field->desc.get());
                auto type = get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed);
                return brgen::concat(type, "(", tmp, ")");
            }
            else {
                return len_field->name;
            }
        }

        result<std::string> handle_encoder_int_length_related(const std::shared_ptr<Field>& field) {
            if (auto vec = field->length_related.lock()) {
                if (config.vector_mode == VectorMode::std_vector) {
                    return handle_encoder_int_length_related_std_vector(field, vec);
                }
                else {
                    assert(config.vector_mode == VectorMode::pointer);
                    return handle_encoder_int_length_related_pointer(field, vec);
                }
            }
            return field->name;
        }

        result<void> convert_bulk(BulkFields* b, Method method) {
            ApplyBulkInt bulk;
            for (auto& field : b->fields) {
                if (field->desc->type == DescType::array_int) {
                    if (auto res = add_bulk_array(bulk, field); !res) {
                        return res.transform(empty_void);
                    }
                }
                else {
                    assert(field->desc->type == DescType::int_);
                    if (method == Method::encode) {
                        auto name = handle_encoder_int_length_related(field);
                        if (!name) {
                            return name.transform(empty_void);
                        }
                        bulk.field_names.push_back(std::move(*name));
                    }
                    else {
                        assert(method == Method::decode || method == Method::move);
                        bulk.field_names.push_back(field->name);
                    }
                }
            }
            events.push_back(std::move(bulk));
            return {};
        }

        result<void> convert_bits(const std::shared_ptr<BitFields>& f, Method m) {
            ApplyBits bits;
            auto primitive = get_primitive_type(ast::aligned_bit(f->fixed_size), false);
            bits.base_type = primitive;
            bits.base_name = f->base_name;
            std::optional<std::string> tmp_copy;
            size_t i = 0;
            for (auto& field : f->fields) {
                assert(field->desc->type == DescType::int_);
                if (m == Method::encode) {
                    auto res = handle_encoder_int_length_related(field);
                    if (!res) {
                        return res.transform(empty_void);
                    }
                    if (*res != field->name) {
                        if (!tmp_copy) {
                            auto tmp = tmp_len_of(bits.base_name);
                            events.push_back(DefineBits{
                                .var_name = tmp,
                                .from = bits.base_name,
                            });
                            bits.base_name = tmp;
                            tmp_copy = tmp;
                        }
                        events.push_back(SetBits{
                            .base_field = *tmp_copy,
                            .bit_field_index = brgen::nums(i),
                            .value = *res,
                        });
                    }
                }
                auto int_desc = static_cast<IntDesc*>(field->desc.get());
                bits.field_names.push_back(BitField{
                    .bit_size = brgen::nums(int_desc->desc.bit_size),
                    .field_name = field->name,
                });
                i++;
            }
            events.push_back(std::move(bits));
            return {};
        }

        result<void> convert_vec_decode(const std::shared_ptr<Field>& vec) {
            auto vec_desc = static_cast<VectorDesc*>(vec->desc.get());
            auto int_desc = static_cast<IntDesc*>(vec_desc->base_type.get());
            auto len_field = vec->length_related.lock();
            auto len_desc = static_cast<IntDesc*>(len_field->desc.get());
            tool::Stringer s;
            auto tmp = tmp_len_of(vec->name);
            std::string replaced = "this->" + len_field->name;
            if (auto bit = len_desc->bit_field.lock()) {
                replaced += "()";
            }
            s.ident_map[len_field->name] = replaced;
            auto length = s.to_string(vec_desc->desc.length);
            events.push_back(DefineLength{
                .field_name = tmp,
                .length = length,
            });
            if (config.vector_mode == VectorMode::std_vector) {
                events.push_back(AssertFalse{
                    .comment = "check size for buffer limit",
                    .cond = brgen::concat(tmp, " > ", vec->name, ".max_size()"),
                });
            }
            if (replaced != length) {
                s.tmp_var_map[0] = tmp;
                length = s.to_string(vec_desc->resolved_expr);
                events.push_back(AssertFalse{
                    .comment = "check overflow",
                    .cond = brgen::concat(len_field->name, "!=", length),
                });
            }
            events.push_back(AllocateVector{
                .field_name = vec->name,
                .element_type = std::string(get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed)),
                .length_var = tmp,
            });
            events.push_back(ApplyVector{
                .field_name = vec->name,
                .length_var = tmp,
            });
            return {};
        }

        result<void> convert_vec_encode(const std::shared_ptr<Field>& vec) {
            if (config.vector_mode == VectorMode::pointer) {
                events.push_back(AssertFalse{
                    .comment = "check null pointer",
                    .cond = brgen::concat(vec->name, " == nullptr"),
                });
                if (config.asymmetric) {
                    events.push_back(ApplyVector{
                        .field_name = vec->name,
                        .length_var = vec->length_related.lock()->name,
                    });
                }
                else {
                    events.push_back(ApplyVector{
                        .field_name = vec->name,
                        .length_var = tmp_len_of(vec->name),
                    });
                }
            }
            else {
                assert(config.vector_mode == VectorMode::std_vector);
                events.push_back(ApplyVector{
                    .field_name = vec->name,
                    .length_var = vec->name + ".size()",
                });
            }
            return {};
        }

        result<void> convert_union_int(MergedFields& mf, const std::shared_ptr<Field>& f, Method m) {
            auto union_int = static_cast<IntUnionDesc*>(f->desc.get());
            auto cond = union_int->desc.cond;
            std::vector<std::shared_ptr<ast::Ident>> ident;
            tool::Stringer s;
            bool second = false;
            bool has_else = false;
            // collect ident
            tool::extract_ident(cond, [&](auto&& i) {
                ident.push_back(std::move(i));
            });
            for (auto& field : union_int->desc.fields) {
                tool::extract_ident(field.cond, [&](auto&& i) {
                    ident.push_back(std::move(i));
                });
            }
            for (auto& i : ident) {
                if (auto found = mf.field_map.find(i->ident); found != mf.field_map.end()) {
                    if (found->second->desc->type == DescType::int_) {
                        auto int_desc = static_cast<IntDesc*>(found->second->desc.get());
                        if (int_desc->bit_field.lock()) {
                            s.ident_map[i->ident] = found->second->name + "()";
                        }
                    }
                }
            }

            for (auto& field : union_int->desc.fields) {
                if (cond) {
                    auto cmp = s.to_string(cond);
                    auto target = s.to_string(field.cond);
                    events.push_back(IfBegin{
                        .cond = brgen::concat(cmp, " == ", target),
                        .elif = second,
                    });
                }
                else if (field.cond) {
                    events.push_back(IfBegin{
                        .cond = s.to_string(field.cond),
                        .elif = second,
                    });
                }
                else {
                    assert(!has_else);
                    has_else = true;
                    events.push_back(ElseBegin{});
                }
                auto tmp_var = tmp_len_of(f->name);
                auto val = get_primitive_type(field.desc.bit_size, field.desc.is_signed);
                if (m == Method::encode) {
                    config.includes.emplace("<limits>");
                    events.push_back(AssertFalse{
                        .comment = "check overflow",
                        .cond = brgen::concat(f->name, " > ", "(std::numeric_limits<", val, ">::max)()"),
                    });
                    events.push_back(DefineLength{
                        .field_name = tmp_var,
                        .length = brgen::concat(val, "(", f->name, ")"),
                    });
                    events.push_back(ApplyInt{
                        .field_name = tmp_var,
                    });
                }
                else if (m == Method::decode) {
                    events.push_back(DefineLength{
                        .field_name = tmp_var,
                        .length = brgen::concat(val, "(", "0", ")"),
                    });
                    events.push_back(ApplyInt{
                        .field_name = tmp_var,
                    });
                    events.push_back(Assign{
                        .to = f->name,
                        .from = tmp_var,
                    });
                }
                events.push_back(BlockEnd{});
                second = true;
            }
            if (!has_else && !union_int->desc.ignore_if_not_match) {
                events.push_back(ElseBegin{});
                events.push_back(ReturnError{
                    .comment = "invalid value",
                });
                events.push_back(BlockEnd{});
            }
            return {};
        }

       public:
        result<void> convert_to_decoder_event(MergedFields& fields) {
            for (auto& field : fields.fields) {
                if (auto f = std::get_if<BulkFields>(&field)) {
                    if (auto res = convert_bulk(f, Method::decode); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto f = std::get_if<std::shared_ptr<BitFields>>(&field)) {
                    if (auto res = convert_bits(*f, Method::decode); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::int_) {
                        auto int_desc = static_cast<IntDesc*>(f->desc.get());
                        events.push_back(ApplyInt{
                            .field_name = f->name,
                        });
                    }
                    else if (f->desc->type == DescType::array_int) {
                        ApplyBulkInt bulk;
                        if (auto res = add_bulk_array(bulk, f); !res) {
                            return res.transform(empty_void);
                        }
                        events.push_back(std::move(bulk));
                    }
                    else if (f->desc->type == DescType::vector) {
                        if (auto res = convert_vec_decode(f); !res) {
                            return res.transform(empty_void);
                        }
                    }
                    else if (f->desc->type == DescType::union_int) {
                        if (auto res = convert_union_int(fields, f, Method::decode); !res) {
                            return res.transform(empty_void);
                        }
                    }
                }
            }
            return {};
        }

        result<void> convert_to_encoder_event(MergedFields& fields) {
            size_t tmp_ = 0;
            for (auto& field : fields.fields) {
                if (auto f = std::get_if<BulkFields>(&field)) {
                    if (auto res = convert_bulk(f, Method::encode); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto f = std::get_if<std::shared_ptr<BitFields>>(&field)) {
                    if (auto res = convert_bits(*f, Method::encode); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::int_) {
                        auto name = handle_encoder_int_length_related(f);
                        if (!name) {
                            return name.transform(empty_void);
                        }
                        auto int_desc = static_cast<IntDesc*>(f->desc.get());
                        events.push_back(ApplyInt{
                            .field_name = std::move(*name),
                        });
                    }
                    else if (f->desc->type == DescType::array_int) {
                        ApplyBulkInt bulk;
                        if (auto res = add_bulk_array(bulk, f); !res) {
                            return res.transform(empty_void);
                        }
                        events.push_back(std::move(bulk));
                    }
                    else if (f->desc->type == DescType::vector) {
                        if (auto res = convert_vec_encode(f); !res) {
                            return res.transform(empty_void);
                        }
                    }
                    else if (f->desc->type == DescType::union_int) {
                        if (auto res = convert_union_int(fields, f, Method::encode); !res) {
                            return res.transform(empty_void);
                        }
                    }
                }
            }
            return {};
        }

        result<void> convert_to_deallocate_event(MergedFields& fields) {
            for (auto& field : fields.fields) {
                if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::vector) {
                        events.push_back(ApplyVector{
                            .field_name = f->name,
                        });
                    }
                }
            }
            return {};
        }

        result<void> convert_to_move_event(MergedFields& fields) {
            for (auto& field : fields.fields) {
                if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::vector) {
                        events.push_back(ApplyVector{
                            .field_name = f->name,
                        });
                    }
                    else if (f->desc->type == DescType::array_int) {
                        ApplyBulkInt bulk;
                        if (auto res = add_bulk_array(bulk, f); !res) {
                            return res.transform(empty_void);
                        }
                        events.push_back(std::move(bulk));
                    }
                    else {
                        assert(f->desc->type == DescType::int_);
                        events.push_back(ApplyInt{
                            .field_name = f->name,
                        });
                    }
                }
                else if (auto bulk = std::get_if<BulkFields>(&field)) {
                    if (auto res = convert_bulk(bulk, Method::move); !res) {
                        return res.transform(empty_void);
                    }
                }
            }
            return {};
        }
    };
}  // namespace json2cpp
