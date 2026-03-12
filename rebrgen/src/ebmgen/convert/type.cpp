#include "../converter.hpp"
#include "core/ast/node/base.h"
#include "core/ast/node/expr.h"
#include "ebm/extended_binary_module.hpp"
#include "helper.hpp"
#include <core/ast/traverse.h>

namespace ebmgen {
    expected<ebm::TypeBody> TypeConverter::convert_function_type(const std::shared_ptr<ast::FunctionType>& n) {
        ebm::TypeBody body;
        body.kind = ebm::TypeKind::FUNCTION;
        ebm::FuncTypeDesc func_desc;
        if (n->return_type) {
            EBMA_CONVERT_TYPE(return_type, n->return_type);
            func_desc.return_type = return_type;
        }
        else {
            EBMU_VOID_TYPE(void_type);
            func_desc.return_type = void_type;
        }

        ebm::Types params;
        for (const auto& param : n->parameters) {
            EBMA_CONVERT_TYPE(param_type, param);
            append(params, param_type);
        }
        func_desc.params = std::move(params);
        func_desc.annotation(ebm::FuncTypeAnnotation::FUNC_PTR);
        body.func_desc(std::move(func_desc));
        return body;
    }

    expected<ebm::TypeRef> TypeConverter::convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field) {
        if (auto found = ctx.state().get_cached_type(type); !is_nil(found)) {
            return found;
        }
        ebm::TypeBody body;
        expected<void> result = {};  // To capture errors from within the lambda

        // special case for StructUnionType
        if (auto n = ast::as<ast::StructUnionType>(type)) {
            MAYBE(varint_id, ctx.repository().new_type_id());
            body.kind = ebm::TypeKind::VARIANT;
            EBMA_CONVERT_STATEMENT(related_field, field);
            ebm::VariantDesc desc;
            desc.related_field = to_weak(related_field);
            ebm::Types members;
            std::optional<std::uint64_t> max_bit_size;
            for (auto& struct_member : n->structs) {
                MAYBE(member_type_ref, ctx.get_statement_converter().convert_struct_decl(struct_member, varint_id));
                ebm::TypeBody body;
                body.kind = ebm::TypeKind::STRUCT;
                body.id(to_weak(member_type_ref));
                EBMA_ADD_TYPE(member_type_ref2, std::move(body));
                append(members, member_type_ref2);
                ctx.state().cache_type(struct_member, member_type_ref2);
                // try get struct size
                MAYBE(struct_stmt, ctx.repository().get_statement(member_type_ref));
                MAYBE(struct_decl, struct_stmt.body.struct_decl());
                if (auto size = struct_decl.size()) {
                    std::uint64_t bit_size = 0;
                    if (size->unit == ebm::SizeUnit::BIT_FIXED) {
                        bit_size = size->size()->value();
                    }
                    else if (size->unit == ebm::SizeUnit::BYTE_FIXED) {
                        bit_size = size->size()->value() * 8;
                    }
                    if (max_bit_size) {
                        max_bit_size = (std::max)(*max_bit_size, bit_size);
                    }
                    else {
                        max_bit_size = bit_size;
                    }
                }
                else {
                    max_bit_size = std::nullopt;  // not fixed
                }
            }
            desc.members = std::move(members);
            if (max_bit_size) {
                EBMU_UINT_TYPE(variant_common, *max_bit_size);
                desc.common_type = variant_common;
            }
            body.variant_desc(std::move(desc));
            EBMA_ADD_TYPE(type_ref, varint_id, std::move(body));
            return type_ref;
        }

        auto fn = [&](auto&& n) -> expected<void> {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<ast::IntType>>) {
                if (n->is_signed) {
                    body.kind = ebm::TypeKind::INT;
                }
                else {
                    body.kind = ebm::TypeKind::UINT;
                }
                if (!n->bit_size) {
                    return unexpect_error("IntType must have a bit_size");
                }
                MAYBE(bit_size, varint(*n->bit_size));
                body.size(bit_size);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::BoolType>>) {
                body.kind = ebm::TypeKind::BOOL;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::FloatType>>) {
                body.kind = ebm::TypeKind::FLOAT;
                if (!n->bit_size) {
                    return unexpect_error("FloatType must have a bit_size");
                }
                MAYBE(bit_size, varint(*n->bit_size));
                body.size(bit_size);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::IdentType>>) {
                if (auto locked = n->base.lock()) {
                    EBMA_CONVERT_TYPE(converted_type, locked, field);
                    body = ctx.repository().get_type(converted_type)->body;  // Copy the body from the converted type
                }
                else {
                    return unexpect_error("IdentType has no base type");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::ArrayType>>) {
                EBMA_CONVERT_TYPE(element_type, n->element_type);
                body.kind = ebm::TypeKind::VECTOR;
                if (n->length_value) {
                    body.kind = ebm::TypeKind::ARRAY;
                    MAYBE(expected_len, varint(*n->length_value));
                    body.length(expected_len);
                }
                else if (is_alignment_vector(field)) {
                    auto vector_len = *field->arguments->alignment_value / 8 - 1;
                    body.kind = ebm::TypeKind::ARRAY;
                    MAYBE(expected_len, varint(vector_len));
                    body.length(expected_len);
                }
                body.element_type(element_type);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::IntLiteralType>>) {
                body.kind = ebm::TypeKind::UINT;  // Assuming unsigned for now
                if (auto locked_literal = n->base.lock()) {
                    auto val = locked_literal->template parse_as<std::uint64_t>();
                    if (!val) {
                        return unexpect_error("Failed to parse IntLiteralType value");
                    }
                    MAYBE(bit_size, varint(*val == 0 ? 1 : futils::binary::log2i(*val)));
                    body.size(bit_size);  // +1 to include the sign bit
                }
                else {
                    return unexpect_error("IntLiteralType has no base literal");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::StrLiteralType>>) {
                body.kind = ebm::TypeKind::ARRAY;
                EBMU_U8(element_type);
                body.element_type(element_type);
                if (n->bit_size) {
                    MAYBE(length, varint(*n->bit_size / 8));
                    body.length(length);
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::EnumType>>) {
                body.kind = ebm::TypeKind::ENUM;
                if (auto locked_enum = n->base.lock()) {
                    const auto _mode = ctx.state().set_current_generate_type(GenerateType::Normal);
                    EBMA_CONVERT_STATEMENT(stmt_ref, locked_enum);  // Convert the enum declaration
                    body.id(to_weak(stmt_ref));                     // Use the ID of the enum declaration
                    if (locked_enum->base_type) {
                        EBMA_CONVERT_TYPE(base_type_ref, locked_enum->base_type);
                        body.base_type(base_type_ref);
                    }
                }
                else {
                    return unexpect_error("EnumType has no base enum");
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::StructType>>) {
                body.kind = n->recursive ? ebm::TypeKind::RECURSIVE_STRUCT : ebm::TypeKind::STRUCT;
                MAYBE(name_ref, ctx.get_statement_converter().convert_struct_decl(n));
                body.id(to_weak(name_ref));
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::VoidType>>) {
                body.kind = ebm::TypeKind::VOID;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::MetaType>>) {
                body.kind = ebm::TypeKind::META;
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::RangeType>>) {
                body.kind = ebm::TypeKind::RANGE;
                if (n->base_type) {
                    EBMA_CONVERT_TYPE(base_type_ref, n->base_type);
                    body.base_type(base_type_ref);
                }
                else {
                    body.base_type(ebm::TypeRef{});
                }
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<ast::FunctionType>>) {
                MAYBE(body_, convert_function_type(n));
                body = std::move(body_);
            }
            else {
                return unexpect_error("Unsupported type for conversion: {}", node_type_to_string(type->node_type));
            }
            return {};  // Success
        };

        brgen::ast::visit(ast::cast_to<ast::Node>(type), [&](auto&& n) {
            result = fn(std::forward<decltype(n)>(n));
        });

        if (!result) {
            return unexpect_error(std::move(result.error()));
        }

        EBMA_ADD_TYPE(ref, std::move(body));

        ctx.repository().add_debug_loc(type->loc, ref);
        ctx.state().cache_type(type, ref);
        return ref;  // Return the type reference
    }

}  // namespace ebmgen
