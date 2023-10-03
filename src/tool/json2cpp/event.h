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

    struct ApplyBulk {
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

    using Event = std::variant<ApplyInt, DefineLength, AssertFalse, ApplyBulk, AllocateVector, ApplyVector>;

    using Events = std::vector<Event>;

    enum class Method {
        encode,
        decode,
    };

    struct EventConverter {
        Config& config;
        Events events;

       private:
        brgen::result<void> add_bulk_array(ApplyBulk& bulk, const std::shared_ptr<Field>& field) {
            auto len = static_cast<IntArrayDesc*>(field->desc.get())->desc.length_eval->get<tool::EResultType::integer>();
            for (auto i = 0; i < len; i++) {
                bulk.field_names.push_back(field->base->ident->ident + "[" + brgen::nums(i) + "]");
            }
            return {};
        }

        brgen::result<std::string> handle_encoder_int_length_related_std_vector(const std::shared_ptr<Field>& field, const std::shared_ptr<Field>& vec) {
            auto desc = static_cast<VectorDesc*>(vec->desc.get());
            tool::Stringer s;
            auto tmp = "tmp_len_" + vec->base->ident->ident;
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
            return tmp;
        }

        brgen::result<std::string> handle_encoder_int_length_related_std_vector(const std::shared_ptr<Field>& field, const std::shared_ptr<Field>& vec) {
            auto desc = static_cast<VectorDesc*>(vec->desc.get());
            tool::Stringer s;
            auto tmp = "tmp_len_" + vec->base->ident->ident;
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
            return tmp;
        }

        brgen::result<std::string> handle_encoder_int_length_related(const std::shared_ptr<Field>& field) {
            if (auto vec = field->length_related.lock()) {
                if (config.vector_mode == VectorMode::std_vector) {
                    return handle_encoder_int_length_related_std_vector(field, vec);
                }
            }
            return field->base->ident->ident;
        }

        brgen::result<void> convert_bulk(BulkFields* b, Method method) {
            ApplyBulk bulk;
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
                        bulk.field_names.push_back(field->base->ident->ident);
                    }
                }
            }
            events.push_back(std::move(bulk));
            return {};
        }

        brgen::result<void> convert_vec_decode(const std::shared_ptr<Field>& vec) {
            auto vec_desc = static_cast<VectorDesc*>(vec->desc.get());
            auto int_desc = static_cast<IntDesc*>(vec_desc->base_type.get());
            auto len_field = vec->length_related.lock();
            tool::Stringer s;
            auto tmp = "tmp_len_" + vec->base->ident->ident;
            s.to_string(vec_desc->desc.length);
            events.push_back(DefineLength{
                .field_name = tmp,
                .length = s.buffer,
            });
            events.push_back(AssertFalse{
                .comment = "check size for buffer limit",
                .cond = brgen::concat(tmp, " > ", vec->base->ident->ident, ".max_size()"),
            });
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
                else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::int_) {
                        auto int_desc = static_cast<IntDesc*>(f->desc.get());
                        events.push_back(ApplyInt{
                            .field_name = f->base->ident->ident,
                        });
                    }
                    else if (f->desc->type == DescType::array_int) {
                        ApplyBulk bulk;
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
                        ApplyBulk bulk;
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
    };
}  // namespace json2cpp
