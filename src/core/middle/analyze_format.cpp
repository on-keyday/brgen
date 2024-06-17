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

        auto type_recursively = [&](auto&& f, const std::shared_ptr<ast::Type>& type) -> void {
            if (auto ident = ast::as<ast::IdentType>(type)) {
                f(f, ident->base.lock());
            }
            else if (auto enum_ = ast::as<ast::EnumType>(type)) {
                auto base_ty = enum_->base.lock()->base_type;
                if (!base_ty) {
                    // pattern: enum without base type
                    add_trait(ast::FormatTrait::description_only);
                    return;
                }
                f(f, base_ty);
            }
            else if (auto struct_ = ast::as<ast::StructType>(type)) {
                if (struct_->bit_alignment != ast::BitAlignment::byte_aligned) {
                    // pattern: struct is not byte-aligned
                    add_trait(ast::FormatTrait::bit_stream);
                }
            }
            else if (auto arr_ = ast::as<ast::ArrayType>(type)) {
                if (arr_->length_value) {
                    // pattern: fixed-size array
                    add_trait(ast::FormatTrait::fixed_array);
                }
                else {
                    // pattern: variable-size array
                    add_trait(ast::FormatTrait::variable_array);
                }
            }
            else if (auto int_ = ast::as<ast::IntType>(type)) {
                add_trait(ast::FormatTrait::fixed_primitive);
                if (*int_->bit_size % 8 != 0) {
                    // pattern: size of field is not byte-aligned
                    add_trait(ast::FormatTrait::bit_field);
                }
                else if (!int_->is_common_supported) {
                    add_trait(ast::FormatTrait::uncommon_size);
                }
            }
            else if (auto float_ = ast::as<ast::FloatType>(type)) {
                add_trait(ast::FormatTrait::fixed_primitive);
                if (*float_->bit_size % 8 != 0) {
                    // pattern: size of field is not byte-aligned
                    add_trait(ast::FormatTrait::bit_field);
                }
                else if (!float_->is_common_supported) {
                    add_trait(ast::FormatTrait::uncommon_size);
                }
            }
        };

        auto element_recursively = [&](auto&& f, const std::shared_ptr<ast::Node>& elm) -> void {
            if (auto fmt = ast::as<ast::Format>(elm)) {
                analyze_format(ast::cast_to<ast::Format>(elm));
            }
            if (auto field = ast::as<ast::Field>(elm)) {
                auto type = field->field_type;
                type_recursively(type_recursively, type);
            }
            if (auto for_ = ast::as<ast::Loop>(elm)) {
                add_trait(ast::FormatTrait::for_loop);
                if (for_->init) {
                    f(f, for_->init);
                }
                if (for_->cond) {
                    f(f, for_->cond);
                }
                if (for_->step) {
                    f(f, for_->step);
                }
                for (auto& elm : for_->body->elements) {
                    f(f, elm);
                }
            }
            if (auto assert_ = ast::as<ast::Assert>(elm)) {
                add_trait(ast::FormatTrait::assertion);
            }
            if (auto explicit_error = ast::as<ast::ExplicitError>(elm)) {
                add_trait(ast::FormatTrait::explicit_error);
            }
            if (auto bin = ast::as<ast::Binary>(elm); bin && ast::is_assign_op(bin->op)) {
                add_trait(ast::FormatTrait::local_variable);
            }
        };

        for (auto& elm : fmt->body->elements) {
            element_recursively(element_recursively, elm);
        }
    }
}  // namespace brgen::middle
