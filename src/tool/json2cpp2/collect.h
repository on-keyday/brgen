/*license*/
#pragma once

#include "cpp_type.h"

namespace j2cp2 {

    struct FormatScanner {
        std::shared_ptr<Struct> struct_;

        void add_field(std::shared_ptr<ast::Field>& f) {
            auto field = std::make_shared<Field>();
            field->base = f;
            struct_->fields.push_back(field);
        }

        void scan(std::shared_ptr<ast::Format>&& f) {
            struct_ = std::make_shared<Struct>(std::move(f));
            std::vector<std::shared_ptr<ast::Field>> fields;
            for (auto& element : f->body->elements) {
                if (ast::as<ast::Field>(element)) {
                    fields.push_back(ast::cast_to<ast::Field>(element));
                }
            }
            for (auto it = fields.begin(); it != fields.end(); ++it) {
                auto& f = *it;
                if (!f->ident) {
                    continue;
                }
                if (auto i_typ = ast::as<ast::IntType>(f->field_type); i_typ && i_typ->is_common_supported || i_typ->bit_size == 128) {
                    auto field = std::make_shared<Field>();
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
                }
                else if (i_typ) {
                    auto bit_flags = std::make_shared<BitFlags>();
                }
            }
        }
    };

    struct Targets {
        std::vector<std::shared_ptr<ast::Format>> formats;
        std::vector<std::shared_ptr<ast::Enum>> enums;

        void collect(std::shared_ptr<ast::Program>&& p) {
            for (auto& element : p->elements) {
                if (ast::as<ast::Format>(element)) {
                    formats.push_back(ast::cast_to<ast::Format>(element));
                }
                else if (ast::as<ast::Enum>(element)) {
                    enums.push_back(ast::cast_to<ast::Enum>(element));
                }
            }
        }
    };

}  // namespace j2cp2
