/*license*/
#pragma once

#include "cpp_type.h"
#include <helper/expected.h>

namespace j2cp2 {

    namespace exp = utils::helper::either;

    struct Error {};

    auto error(Error e) {
        return exp::unexpected(e);
    }

    struct Scanner {
        template <class It>
        void scan_bit_range(It begin, It end) {
            for (auto it = begin; it != end; it++) {
            }
        }

        exp::expected<std::shared_ptr<Union>, Error> scan_union_struct(std::shared_ptr<ast::StructUnionType>& s_union) {
            auto union_ = std::make_shared<Union>();
            union_->base = s_union;
            for (auto& field : s_union->fields) {
                auto s = scan(field);
                if (!s) {
                    return error(s.error());
                }
                union_->fields.push_back(*s);
            }
            return union_;
        }

        exp::expected<std::shared_ptr<Struct>, Error> scan(std::shared_ptr<ast::StructType>& f) {
            auto struct_ = std::make_shared<Struct>(std::move(f));
            std::vector<std::shared_ptr<ast::Field>> fields;
            for (auto& element : f->fields) {
                if (ast::as<ast::Field>(element)) {
                    fields.push_back(ast::cast_to<ast::Field>(element));
                }
            }
            for (auto it = fields.begin(); it != fields.end(); ++it) {
                auto& f = *it;
                if (auto s_union_ = ast::as<ast::StructUnionType>(f->field_type); s_union_) {
                    auto s_union = scan_union_struct(ast::cast_to<ast::StructUnionType>(f->field_type));
                    if (!s_union) {
                        return error(s_union.error());
                    }
                    auto field = std::make_shared<Field>();
                    field->base = f;
                    field->type = *s_union;
                    field->name = "";  // anonymous union
                    struct_->fields.push_back(std::move(field));
                    continue;
                }
                if (ast::as<ast::UnionType>(f->field_type)) {
                    continue;  // skip union candidate
                }
                if (auto i_typ = ast::as<ast::IntType>(f->field_type); i_typ && i_typ->is_common_supported || i_typ->bit_size == 128) {
                    auto field = std::make_shared<Field>();
                    if (f->ident) {
                        field->name = f->ident->ident;
                    }
                    else {
                        field->name = "";  // anonymous field
                    }
                    field->base = f;
                    auto ty = std::make_shared<Integer>();
                    switch (i_typ->bit_size) {
                        case 8:
                            ty->base_type = BuiltinType::uint8;
                            break;
                        case 16:
                            ty->base_type = BuiltinType::uint16;
                            break;
                        case 32:
                            ty->base_type = BuiltinType::uint32;
                            break;
                        case 64:
                            ty->base_type = BuiltinType::uint64;
                            break;
                        case 128:
                            ty->base_type = BuiltinType::uint128;
                            break;
                        default:
                            assert(false);
                            __builtin_unreachable();
                    }
                    ty->is_signed = i_typ->is_signed;
                    field->type = ty;
                    struct_->fields.push_back(std::move(field));
                }
                else if (i_typ) {
                    // first check range of integer field sequence of fields
                    // allow not aligned,
                    auto it2 = it;
                    size_t bit_sum = 0;
                    for (; it2 != fields.end(); ++it2) {
                        auto& f2 = *it2;
                        if (auto s_union2_ = ast::as<ast::StructUnionType>(f2->field_type); s_union2_) {
                        }
                        if (auto i_typ2 = ast::as<ast::IntType>(f2->field_type)) {
                            bit_sum += i_typ2->bit_size;
                        }
                        break;
                    }
                }
            }
        }
    };

    struct Targets {
        std::vector<std::shared_ptr<ast::Format>> formats;
        std::vector<std::shared_ptr<ast::Enum>> enums;

        exp::expected<> collect(std::shared_ptr<ast::Program>&& p) {
            for (auto& element : p->elements) {
                if (ast::as<ast::Format>(element)) {
                    formats.push_back(ast::cast_to<ast::Format>(element));
                }
                else if (ast::as<ast::Enum>(element)) {
                    enums.push_back(ast::cast_to<ast::Enum>(element));
                }
            }
            Scanner scan;
            for (auto& f : formats) {
                auto s = scan.scan(f);
                if (!s) {
                }
            }
        }
    };

}  // namespace j2cp2
