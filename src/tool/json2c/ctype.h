/*license*/
#pragma once
#include <memory>
#include <string>
#include <core/ast/ast.h>

namespace json2c {
    namespace ast = brgen::ast;
    enum class CTypeKind {
        primitive,
        array,
        pointer,
    };

    // type of c language
    struct CType {
        const CTypeKind kind;
        const std::shared_ptr<ast::Type> base;

        std::string to_string(std::string ident) const {
            auto [prefix, suffix] = to_string_prefix_suffix();
            return prefix + " " + ident + suffix;
        }

        virtual std::pair<std::string, std::string> to_string_prefix_suffix() const = 0;

       protected:
        CType(CTypeKind kind, std::shared_ptr<ast::Type> base)
            : kind(kind), base(base) {}
    };

    struct CPrimitive : CType {
        const std::string name;

        CPrimitive(std::shared_ptr<ast::Type> ty, std::string name)
            : CType(CTypeKind::primitive, ty), name(name) {}

        std::pair<std::string, std::string> to_string_prefix_suffix() const override {
            return {name, ""};
        }
    };

    struct CArray : CType {
        const std::shared_ptr<CType> element_type;
        const std::string length;

        CArray(std::shared_ptr<ast::Type> ty, std::shared_ptr<CType> element_type, std::string length)
            : CType(CTypeKind::array, ty), element_type(element_type), length(length) {}

        std::pair<std::string, std::string> to_string_prefix_suffix() const override {
            auto [element_prefix, element_suffix] = element_type->to_string_prefix_suffix();
            element_suffix = "[" + length + "]" + element_suffix;
            return {element_prefix, element_suffix};
        }
    };

    struct CPointer : CType {
        const std::shared_ptr<CType> element_type;

        CPointer(std::shared_ptr<ast::Type> ty, std::shared_ptr<CType> element_type)
            : CType(CTypeKind::pointer, ty), element_type(element_type) {}

        std::pair<std::string, std::string> to_string_prefix_suffix() const override {
            auto [element_prefix, element_suffix] = element_type->to_string_prefix_suffix();
            if (element_type->kind == CTypeKind::array) {
                return {element_prefix + "(*", ")" + element_suffix};
            }
            return {element_prefix + "*", element_suffix};
        }
    };

    std::shared_ptr<CType> get_type(const std::shared_ptr<ast::Type>& typ) {
        if (auto int_ty = ast::as<ast::IntType>(typ)) {
            auto bit = *int_ty->bit_size;
            if (int_ty->is_common_supported) {
                return std::make_shared<CPrimitive>(typ, brgen::concat(int_ty->is_signed ? "" : "u", "int", brgen::nums(bit), "_t"));
            }
            else if (bit < 64) {
                bit = brgen::ast::aligned_bit(bit);
                return std::make_shared<CPrimitive>(typ, brgen::concat("uint", brgen::nums(bit), "_t"));
            }
        }
        if (auto enum_ty = ast::as<ast::EnumType>(typ)) {
            auto name = enum_ty->base.lock()->ident->ident;
            return std::make_shared<CPrimitive>(typ, brgen::concat("enum ", name));
        }
        if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
            auto element_type = get_type(arr_ty->element_type);
            if (arr_ty->length_value) {
                return std::make_shared<CArray>(typ, element_type, brgen::nums(*arr_ty->length_value));
            }
            else {
                return std::make_shared<CPointer>(typ, element_type);
            }
        }
        if (auto struct_ty = ast::as<ast::StructType>(typ)) {
            auto member = ast::as<ast::Member>(struct_ty->base.lock());
            if (member) {
                return std::make_shared<CPrimitive>(typ, brgen::concat("struct ", member->ident->ident));
            }
        }
        if (auto ident_ty = ast::as<ast::IdentType>(typ)) {
            return get_type(ident_ty->base.lock());
        }
        return nullptr;
    }

}  // namespace json2c
