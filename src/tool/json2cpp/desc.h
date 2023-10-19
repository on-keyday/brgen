/*license*/
#pragma once
#include <core/ast/tool/tool.h>
#include "config.h"

namespace json2cpp {

    enum class DescType {
        array_int,
        vector,
        int_,
        union_int,
    };

    struct Desc {
        const DescType type;
        constexpr Desc(DescType type)
            : type(type) {}
    };

    struct BitFields;

    struct IntDesc : Desc {
        tool::IntDesc desc;
        std::weak_ptr<BitFields> bit_field;

        constexpr IntDesc(tool::IntDesc desc)
            : Desc(DescType::int_), desc(desc) {}
    };

    struct IntArrayDesc : Desc {
        tool::ArrayDesc desc;
        std::shared_ptr<Desc> base_type;
        IntArrayDesc(tool::ArrayDesc desc, std::shared_ptr<Desc> base_type)
            : Desc(DescType::array_int), desc(desc), base_type(std::move(base_type)) {}
    };

    struct Field;

    struct VectorDesc : Desc {
        tool::ArrayDesc desc;
        std::shared_ptr<Desc> base_type;
        std::shared_ptr<ast::Expr> resolved_expr;
        VectorDesc(tool::ArrayDesc desc, std::shared_ptr<Desc> base_type)
            : Desc(DescType::vector), desc(desc), base_type(std::move(base_type)) {}
    };

    struct IntUnionDesc : Desc {
        std::vector<std::shared_ptr<IntDesc>> int_fields;
        IntUnionDesc()
            : Desc(DescType::union_int) {}
    };

    struct Field {
        std::shared_ptr<ast::Field> base;
        std::string name;
        std::shared_ptr<Desc> desc;
        std::weak_ptr<Field> length_related;

        Field(std::shared_ptr<ast::Field> base, std::shared_ptr<Desc> desc)
            : base(std::move(base)), desc(std::move(desc)) {
            name = this->base->ident->ident;
        }
    };

    using Fields = std::vector<std::shared_ptr<Field>>;

    std::string_view get_primitive_type(size_t bit_size, bool is_signed) {
        switch (bit_size) {
            case 8:
                if (is_signed) {
                    return "std::int8_t";
                }
                else {
                    return "std::uint8_t";
                }
            case 16:
                if (is_signed) {
                    return "std::int16_t";
                }
                else {
                    return "std::uint16_t";
                }
            case 32:
                if (is_signed) {
                    return "std::int32_t";
                }
                else {
                    return "std::uint32_t";
                }
            case 64:
                if (is_signed) {
                    return "std::int64_t";
                }
                else {
                    return "std::uint64_t";
                }
            default:
                return {};
        }
    }

    // read/write by binary::[write/read]_num_bulk
    // only contains integer or array of integer
    struct BulkFields {
        std::vector<std::shared_ptr<Field>> fields;
    };

    struct BitFields {
        std::vector<std::shared_ptr<Field>> fields;
        size_t fixed_size = 0;
        size_t primitive_type = 0;
        std::string base_name;
    };

    using MergedField = std::variant<std::shared_ptr<Field>, BulkFields, std::shared_ptr<BitFields>>;

    using MergedFields = std::vector<MergedField>;

    struct FieldCollector {
        Config& config;
        tool::Evaluator& eval;

        FieldCollector(Config& c, tool::Evaluator& e)
            : config(c), eval(e) {}

        MergedFields m;

        result<void> collect(const std::shared_ptr<ast::Format>& fmt) {
            auto& ast_fields = fmt->struct_type->fields;
            for (auto& f : ast_fields) {
                if (ast::as<ast::Field>(f)) {
                    if (auto r = collect_field(fmt, ast::cast_to<ast::Field>(f)); !r) {
                        return r.transform(empty_void);
                    }
                }
            }
            return merge_fields();
        }

       private:
        Fields fields;
        // 可変長配列の長さを解決する
        // 可変長配列の長さが１次方程式(かつ変数が1回のみ(現状x+xなどは無理))で解ける場合のみ解決できる
        // それ以外の場合はエラー
        result<void> collect_vector_field(tool::IntDesc& b, tool::ArrayDesc& a, const std::shared_ptr<ast::Format>& fmt, std::shared_ptr<ast::Field>&& field) {
            if (config.vector_mode == VectorMode::std_vector) {
                config.includes.emplace("<vector>");
            }
            auto vec = std::make_shared<VectorDesc>(std::move(a), std::make_shared<IntDesc>(std::move(b)));
            tool::LinerResolver resolver;
            if (!resolver.resolve(vec->desc.length)) {
                return error(field->loc, "cannot resolve vector length");
            }
            if (tool::belong_format(resolver.x) != fmt) {
                return error(field->loc, "cannot resolve vector length");
            }
            vec->resolved_expr = std::move(resolver.resolved);
            auto vec_field = std::make_shared<Field>(field, std::move(vec));
            bool ok = false;
            for (auto& f : fields) {
                if (f->name == resolver.x->ident) {
                    if (f->length_related.lock()) {
                        return error(field->loc, "cannot resolve vector length");
                    }
                    f->length_related = vec_field;
                    vec_field->length_related = f;
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                return error(field->loc, "cannot resolve vector length");
            }
            fields.push_back(std::move(vec_field));
            return {};
        }

        result<void> collect_field(const std::shared_ptr<ast::Format>& fmt, std::shared_ptr<ast::Field>&& field) {
            auto& fname = field->ident->ident;
            if (auto a = tool::is_array_type(field->field_type, eval)) {
                auto b = tool::is_int_type(a->base_type);
                if (!b) {
                    return error(field->loc, "unsupported type");
                }
                auto type = get_primitive_type(b->bit_size, b->is_signed);
                if (type.empty()) {
                    return error(field->loc, "unsupported type");
                }
                if (a->length_eval) {
                    // 固定長配列
                    config.includes.emplace("<array>");
                    fields.push_back(std::make_shared<Field>(field, std::make_shared<IntArrayDesc>(std::move(*a), std::make_shared<IntDesc>(std::move(*b)))));
                }
                else {
                    if (auto res = collect_vector_field(*b, *a, fmt, std::move(field)); !res) {
                        return res.transform(empty_void);
                    }
                }
            }
            else if (auto i = tool::is_int_type(field->field_type)) {
                // both 8-byte aligned (8*2^n (0<=n<=3)) and non-aligned int are supported
                fields.push_back(std::make_shared<Field>(field, std::make_shared<IntDesc>(std::move(*i))));
            }
            else if (auto i = tool::is_int_union_type(field->field_type)) {
                auto u = std::make_shared<IntUnionDesc>();
                std::string name;
                for (auto& f : i->fields) {
                    if (name.empty()) {
                        name = f->ident->ident;
                    }
                    else {
                        if (name != f->ident->ident) {
                            return error(field->loc, "unsupported type");
                        }
                    }
                    auto b = tool::is_int_type(f->field_type);
                    if (!b) {
                        return error(field->loc, "unsupported type");
                    }
                    auto type = get_primitive_type(b->bit_size, b->is_signed);
                    if (type.empty()) {
                        return error(field->loc, "unsupported type");
                    }
                    u->int_fields.push_back(std::make_shared<IntDesc>(std::move(*b)));
                }
                fields.push_back(std::make_shared<Field>(field, std::move(u)));
            }
            else {
                return error(field->loc, "unsupported type");
            }
            return {};
        }

        result<void> merge_fields() {
            for (size_t i = 0; i < fields.size(); i++) {
                if (fields[i]->desc->type == DescType::vector) {
                    m.push_back({std::move(fields[i])});
                    continue;
                }
                std::shared_ptr<BitFields> bit;
                for (; i < fields.size(); i++) {
                    if (fields[i]->desc->type == DescType::int_) {
                        auto i_desc = static_cast<IntDesc*>(fields[i]->desc.get());
                        auto primitive = get_primitive_type(i_desc->desc.bit_size, i_desc->desc.is_signed);
                        if (primitive.empty()) {
                            if (!bit) {
                                bit = std::make_shared<BitFields>();
                                bit->base_name = "flags";
                            }
                            i_desc->bit_field = bit;
                            bit->base_name += "_" + fields[i]->name;
                            bit->fields.push_back({std::move(fields[i])});
                            bit->fixed_size += i_desc->desc.bit_size;
                            continue;
                        }
                        else if (bit && bit->fixed_size % 8 != 0) {
                            i_desc->bit_field = bit;
                            bit->base_name += "_" + fields[i]->name;
                            bit->fields.push_back({std::move(fields[i])});
                            bit->fixed_size += i_desc->desc.bit_size;
                            continue;
                        }
                    }
                    break;
                }
                if (bit) {
                    if (bit->fixed_size % 8 != 0) {
                        return error(bit->fields[0]->base->loc, "bit field is not byte aligned");
                    }
                    if (bit->fixed_size > 64) {
                        return error(bit->fields[0]->base->loc, "bit field is too large");
                    }
                    m.push_back({std::move(bit)});
                    i--;
                    continue;
                }

                BulkFields b;
                for (; i < fields.size(); i++) {
                    if (fields[i]->desc->type == DescType::array_int) {
                        b.fields.push_back({std::move(fields[i])});
                    }
                    else if (fields[i]->desc->type == DescType::int_) {
                        b.fields.push_back({std::move(fields[i])});
                    }
                    else {
                        break;
                    }
                }
                if (b.fields.size() == 1) {
                    m.push_back({std::move(b.fields[0])});
                }
                else {
                    m.push_back({std::move(b)});
                }
                i--;
            }
            return {};
        }
    };

}  // namespace json2cpp
