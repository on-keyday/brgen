/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>

namespace brgen::middle {

    void analyze_expr(const std::shared_ptr<ast::Expr>& expr, auto&& add_trait) {
        ast::traverse(expr, [&](const std::shared_ptr<ast::Node>& node) {
            if (ast::as<ast::Type>(node)) {
                return;
            }
            if (auto call = ast::as<ast::Call>(node)) {
                if (auto io_op = ast::as<ast::IOOperation>(call->callee)) {
                    if (io_op->method == ast::IOMethod::input_backward) {
                        add_trait(ast::FormatTrait::backward_input);
                    }
                    else if (io_op->method == ast::IOMethod::input_peek) {
                        add_trait(ast::FormatTrait::static_peek);
                    }
                }
            }
        });
    }

    void analyze_field_argument(const std::shared_ptr<ast::FieldArgument>& args, auto&& add_trait) {
        if (args->peek_value && *args->peek_value) {
            add_trait(ast::FormatTrait::static_peek);
        }
        if (args->collected_arguments.size()) {
        }
    }

    void analyze_format(const std::shared_ptr<ast::Format>& fmt) {
        auto add_trait = [&](ast::FormatTrait t) {
            fmt->format_trait = ast::FormatTrait(size_t(fmt->format_trait) | size_t(t));
        };
        auto is = [&](ast::FormatTrait t) {
            return (size_t(fmt->format_trait) & size_t(t)) != 0;
        };
        if (fmt->encode_fn.lock()) {
            add_trait(ast::FormatTrait::procedural);
        }
        if (fmt->decode_fn.lock()) {
            add_trait(ast::FormatTrait::procedural);
        }
        if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
            // pattern: struct is not byte-aligned
            add_trait(ast::FormatTrait::bit_stream);
        }

        auto type_recursively = [&](auto&& f, const std::shared_ptr<ast::Type>& type, bool indirect) -> void {
            if (auto ident = ast::as<ast::IdentType>(type)) {
                f(f, ident->base.lock(), indirect);
            }
            else if (auto enum_ = ast::as<ast::EnumType>(type)) {
                auto base_ty = enum_->base.lock()->base_type;
                if (!base_ty) {
                    // pattern: enum without base type
                    add_trait(ast::FormatTrait::description_only);
                    return;
                }
                f(f, base_ty, indirect);
            }
            else if (auto struct_ = ast::as<ast::StructType>(type)) {
                add_trait(ast::FormatTrait::struct_);
                if (struct_->bit_alignment != ast::BitAlignment::byte_aligned) {
                    if (!is(ast::FormatTrait::bit_stream) && !indirect) {
                    }
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
                f(f, arr_->element_type, true);
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
                type_recursively(type_recursively, type, false);
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
            if (auto if_ = ast::as<ast::If>(elm)) {
                add_trait(ast::FormatTrait::conditional);
                if (if_->cond) {
                    f(f, if_->cond);
                }
                for (auto& elm : if_->then->elements) {
                    f(f, elm);
                }
                if (if_->els) {
                    if (auto els_ = ast::as<ast::If>(if_->els)) {
                        f(f, if_->els);
                    }
                    else if (auto body = ast::as<ast::IndentBlock>(if_->els)) {
                        for (auto& elm : body->elements) {
                            f(f, elm);
                        }
                    }
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
            if (auto specify_order = ast::as<ast::SpecifyOrder>(elm)) {
                if (!specify_order->order_value) {
                    // pattern: order is not constant
                    add_trait(ast::FormatTrait::dynamic_order);
                }
            }
        };

        for (auto& elm : fmt->body->elements) {
            element_recursively(element_recursively, elm);
        }
    }
}  // namespace brgen::middle
