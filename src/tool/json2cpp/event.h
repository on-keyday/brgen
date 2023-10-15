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

    using Event = std::variant<ApplyInt, DefineLength, AssertFalse, ApplyBits, ApplyBulkInt, AllocateVector, ApplyVector>;

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

        result<std::string> handle_encoder_int_length_related_std_vector(
            const std::shared_ptr<Field>& len_field, const std::shared_ptr<Field>& vec_field) {
            auto desc = static_cast<VectorDesc*>(vec_field->desc.get());
            tool::Stringer s;
            auto length = tmp_len_of(vec_field->name);
            auto base = vec_field->name + ".size()";
            s.tmp_var_map[0] = base;
            auto resolved = s.to_string(desc->resolved_expr);
            events.push_back(DefineLength{
                .field_name = length,
                .length = resolved,
            });
            auto int_desc = static_cast<IntDesc*>(len_field->desc.get());
            config.includes.emplace("<limits>");
            events.push_back(AssertFalse{
                .comment = "check overflow",
                .cond = brgen::concat(length, " > ", "(std::numeric_limits<", std::string(get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed)), ">::max)()"),
            });
            if (resolved != base) {
                s.ident_map[len_field->name] = length;
                resolved = s.to_string(desc->desc.length);
                events.push_back(AssertFalse{
                    .comment = "check truncated",
                    .cond = brgen::concat(base, " != ", resolved),
                });
            }
            auto type = get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed);
            return brgen::concat(type, "(", length, ")");
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
            auto int_desc = static_cast<IntDesc*>(len_field->desc.get());
            config.includes.emplace("<limits>");
            events.push_back(AssertFalse{
                .comment = "check overflow",
                .cond = brgen::concat(tmp, " > ", "(std::numeric_limits<", std::string(get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed)), ">::max)()"),
            });
            if (length != base) {
                length = s.to_string(check_length);
                events.push_back(AssertFalse{
                    .comment = "check truncated",
                    .cond = brgen::concat(base, " != ", length),
                });
            }
            auto type = get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed);
            if (config.asymmetric) {
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
            for (auto& field : f->fields) {
                assert(field->desc->type == DescType::int_);
                if (m == Method::encode) {
                    handle_encoder_int_length_related(field);
                }
                auto int_desc = static_cast<IntDesc*>(field->desc.get());
                bits.field_names.push_back(BitField{
                    .bit_size = brgen::nums(int_desc->desc.bit_size),
                    .field_name = field->name,
                });
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
            if (auto bit = len_desc->bit_field.lock()) {
                s.ident_map[len_field->name] = "this->" + len_field->name + "()";
            }
            else {
                s.ident_map[len_field->name] = "this->" + len_field->name;
            }
            auto length = s.to_string(vec_desc->resolved_expr);
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
            if (tmp != length) {
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

       public:
        result<void> convert_to_decoder_event(MergedFields& fields) {
            for (auto& field : fields) {
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
                    else {
                        if (auto res = convert_vec_decode(f); !res) {
                            return res.transform(empty_void);
                        }
                    }
                }
            }
            return {};
        }

        result<void> convert_to_encoder_event(MergedFields& fields) {
            size_t tmp_ = 0;
            for (auto& field : fields) {
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
                    else {
                        assert(f->desc->type == DescType::vector);
                        if (auto res = convert_vec_encode(f); !res) {
                            return res.transform(empty_void);
                        }
                    }
                }
            }
            return {};
        }

        result<void> convert_to_deallocate_event(MergedFields& fields) {
            for (auto& field : fields) {
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
            for (auto& field : fields) {
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
