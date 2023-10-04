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

        brgen::result<void> add_bulk_array(ApplyBulkInt& bulk, const std::shared_ptr<Field>& field) {
            auto len = static_cast<IntArrayDesc*>(field->desc.get())->desc.length_eval->get<tool::EResultType::integer>();
            for (auto i = 0; i < len; i++) {
                bulk.field_names.push_back(field->base->ident->ident + "[" + brgen::nums(i) + "]");
            }
            return {};
        }

        brgen::result<std::string> handle_encoder_int_length_related_std_vector(const std::shared_ptr<Field>& field, const std::shared_ptr<Field>& vec) {
            auto desc = static_cast<VectorDesc*>(vec->desc.get());
            tool::Stringer s;
            auto tmp = tmp_len_of(vec->base->ident->ident);
            auto base = vec->base->ident->ident + ".size()";
            s.tmp_var_map[0] = base;
            s.to_string(desc->resolved_expr);
            events.push_back(DefineLength{
                .field_name = tmp,
                .length = std::move(s.buffer),
            });
            auto int_desc = static_cast<IntDesc*>(field->desc.get());
            config.includes.emplace("<limits>");
            events.push_back(AssertFalse{
                .comment = "check overflow",
                .cond = brgen::concat(tmp, " > ", "(std::numeric_limits<", std::string(get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed)), ">::max)()"),
            });
            if (s.buffer != base) {
                s.buffer.clear();
                s.ident_map[field->base->ident->ident] = tmp;
                s.to_string(desc->desc.length);
                events.push_back(AssertFalse{
                    .comment = "check truncated",
                    .cond = brgen::concat(base, " != ", s.buffer),
                });
            }
            auto type = get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed);
            return brgen::concat(type, "(", tmp, ")");
        }

        brgen::result<std::string> handle_encoder_int_length_related_pointer(const std::shared_ptr<Field>& field, const std::shared_ptr<Field>& vec) {
            auto desc = static_cast<VectorDesc*>(vec->desc.get());
            tool::Stringer s;
            auto tmp = tmp_len_of(vec->base->ident->ident);
            auto base = brgen::concat("size_t(", field->base->ident->ident, ")");
            std::shared_ptr<ast::Expr> length, check_length;
            if (config.asymmetric) {
                s.ident_map[field->base->ident->ident] = tmp;
                s.tmp_var_map[0] = base;
                length = desc->resolved_expr;
                check_length = desc->desc.length;
            }
            else {
                s.tmp_var_map[0] = tmp;
                s.ident_map[field->base->ident->ident] = base;
                length = desc->desc.length;
                check_length = desc->resolved_expr;
            }
            s.to_string(length);
            events.push_back(DefineLength{
                .field_name = tmp,
                .length = std::move(s.buffer),
            });
            auto int_desc = static_cast<IntDesc*>(field->desc.get());
            config.includes.emplace("<limits>");
            events.push_back(AssertFalse{
                .comment = "check overflow",
                .cond = brgen::concat(tmp, " > ", "(std::numeric_limits<", std::string(get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed)), ">::max)()"),
            });
            if (s.buffer != base) {
                s.buffer.clear();
                s.to_string(check_length);
                events.push_back(AssertFalse{
                    .comment = "check truncated",
                    .cond = brgen::concat(base, " != ", s.buffer),
                });
            }
            auto type = get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed);
            if (config.asymmetric) {
                return brgen::concat(type, "(", tmp, ")");
            }
            else {
                return field->base->ident->ident;
            }
        }

        brgen::result<std::string> handle_encoder_int_length_related(const std::shared_ptr<Field>& field) {
            if (auto vec = field->length_related.lock()) {
                if (config.vector_mode == VectorMode::std_vector) {
                    return handle_encoder_int_length_related_std_vector(field, vec);
                }
                else {
                    assert(config.vector_mode == VectorMode::pointer);
                    return handle_encoder_int_length_related_pointer(field, vec);
                }
            }
            return field->base->ident->ident;
        }

        brgen::result<void> convert_bulk(BulkFields* b, Method method) {
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
                        bulk.field_names.push_back(field->base->ident->ident);
                    }
                }
            }
            events.push_back(std::move(bulk));
            return {};
        }

        brgen::result<void> convert_bits(BitFields* f) {
            ApplyBits bits;
            auto primitive = get_primitive_type(ast::aligned_bit(f->fixed_size), false);
            bits.base_type = primitive;
            bits.base_name = "flags";
            for (auto& field : f->fields) {
                assert(field->desc->type == DescType::int_);
                auto int_desc = static_cast<IntDesc*>(field->desc.get());
                bits.field_names.push_back(BitField{
                    .bit_size = brgen::nums(int_desc->desc.bit_size),
                    .field_name = field->base->ident->ident,
                });
                bits.base_name += "_" + field->base->ident->ident;
            }
            events.push_back(std::move(bits));
            return {};
        }

        brgen::result<void> convert_vec_decode(const std::shared_ptr<Field>& vec) {
            auto vec_desc = static_cast<VectorDesc*>(vec->desc.get());
            auto int_desc = static_cast<IntDesc*>(vec_desc->base_type.get());
            auto len_field = vec->length_related.lock();
            tool::Stringer s;
            auto tmp = tmp_len_of(vec->base->ident->ident);
            s.to_string(vec_desc->desc.length);
            events.push_back(DefineLength{
                .field_name = tmp,
                .length = s.buffer,
            });
            if (config.vector_mode == VectorMode::std_vector) {
                events.push_back(AssertFalse{
                    .comment = "check size for buffer limit",
                    .cond = brgen::concat(tmp, " > ", vec->base->ident->ident, ".max_size()"),
                });
            }
            if (tmp != s.buffer) {
                s.buffer.clear();
                s.tmp_var_map[0] = tmp;
                s.to_string(vec_desc->resolved_expr);
                events.push_back(AssertFalse{
                    .comment = "check overflow",
                    .cond = brgen::concat(len_field->base->ident->ident, "!=", s.buffer),
                });
            }
            events.push_back(AllocateVector{
                .field_name = vec->base->ident->ident,
                .element_type = std::string(get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed)),
                .length_var = tmp,
            });
            events.push_back(ApplyVector{
                .field_name = vec->base->ident->ident,
                .length_var = tmp,
            });
            return {};
        }

        brgen::result<void> convert_vec_encode(const std::shared_ptr<Field>& vec) {
            if (config.vector_mode == VectorMode::pointer) {
                events.push_back(AssertFalse{
                    .comment = "check null pointer",
                    .cond = brgen::concat(vec->base->ident->ident, " == nullptr"),
                });
                if (config.asymmetric) {
                    events.push_back(ApplyVector{
                        .field_name = vec->base->ident->ident,
                        .length_var = vec->length_related.lock()->base->ident->ident,
                    });
                }
                else {
                    events.push_back(ApplyVector{
                        .field_name = vec->base->ident->ident,
                        .length_var = tmp_len_of(vec->base->ident->ident),
                    });
                }
            }
            else {
                assert(config.vector_mode == VectorMode::std_vector);
                events.push_back(ApplyVector{
                    .field_name = vec->base->ident->ident,
                    .length_var = vec->base->ident->ident + ".size()",
                });
            }
            return {};
        }

       public:
        brgen::result<void> convert_to_decoder_event(MergedFields& fields) {
            for (auto& field : fields) {
                if (auto f = std::get_if<BulkFields>(&field)) {
                    if (auto res = convert_bulk(f, Method::decode); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto f = std::get_if<BitFields>(&field)) {
                    if (auto res = convert_bits(f); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::int_) {
                        auto int_desc = static_cast<IntDesc*>(f->desc.get());
                        events.push_back(ApplyInt{
                            .field_name = f->base->ident->ident,
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

        brgen::result<void> convert_to_encoder_event(MergedFields& fields) {
            size_t tmp_ = 0;
            for (auto& field : fields) {
                if (auto f = std::get_if<BulkFields>(&field)) {
                    if (auto res = convert_bulk(f, Method::encode); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto f = std::get_if<BitFields>(&field)) {
                    if (auto res = convert_bits(f); !res) {
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

        brgen::result<void> convert_to_deallocate_event(MergedFields& fields) {
            for (auto& field : fields) {
                if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::vector) {
                        events.push_back(ApplyVector{
                            .field_name = f->base->ident->ident,
                        });
                    }
                }
            }
            return {};
        }

        brgen::result<void> convert_to_move_event(MergedFields& fields) {
            for (auto& field : fields) {
                if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::vector) {
                        events.push_back(ApplyVector{
                            .field_name = f->base->ident->ident,
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
                            .field_name = f->base->ident->ident,
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
