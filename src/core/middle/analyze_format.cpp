/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>

namespace brgen::middle {
    void analyze_format(const std::shared_ptr<ast::Format>& fmt) {
        auto add_trait = [&](ast::FormatTrait t) {
            fmt->format_trait = ast::FormatTrait(size_t(fmt->format_trait) | size_t(t));
        };
        if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
            // pattern: struct is not byte-aligned
            add_trait(ast::FormatTrait::bit_stream);
        }
        for (auto& elm : fmt->body->elements) {
            if (auto field = ast::as<ast::Field>(elm)) {
                auto type = field->field_type;
                if (auto ident = ast::as<ast::IdentType>(type)) {
                    type = ident->base.lock();
                }
                if (auto enum_ = ast::as<ast::EnumType>(type)) {
                    auto base_ty = enum_->base.lock()->base_type;
                    if (!enum_->bit_size) {
                        // pattern: size of enum without base type cannot determine size of field
                        add_trait(ast::FormatTrait::description_only);
                    }
                    if (auto ident = ast::as<ast::IdentType>(base_ty)) {
                        base_ty = ident->base.lock();
                    }
                    type = base_ty;
                }
                if (auto int_ = ast::as<ast::IntType>(type)) {
                    add_trait(ast::FormatTrait::fixed_primitive);
                    if (*int_->bit_size % 8 != 0) {
                        // pattern: size of field is not byte-aligned
                        add_trait(ast::FormatTrait::bit_field);
                    }
                }
                if (auto struct_ = ast::as<ast::StructType>(type)) {
                    add_trait(ast::FormatTrait::struct_);
                }
            }
        }
    }
}  // namespace brgen::middle
