/*license*/
#pragma once

#include "../codegen.hpp"
#include "ebm/extended_binary_module.hpp"

namespace std {
    template <>
    struct hash<std::pair<size_t, ebm::TypeRef>> {
        size_t operator()(const std::pair<size_t, ebm::TypeRef>& p) const {
            return std::hash<size_t>()(p.first) ^ std::hash<ebm::TypeRef>()(p.second);
        }
    };
}  // namespace std

namespace ebm2rmw {

    struct TypeLayout {
        ebm::TypeRef type;
        size_t size = 0;

        TypeLayout() = default;

        explicit TypeLayout(ebm::TypeRef type, size_t size)
            : type(type), size(size) {}
    };

    struct FieldLayout {
        ebm::StatementRef field;
        TypeLayout type;
        size_t offset = 0;

        FieldLayout(ebm::StatementRef field, TypeLayout type_layout, size_t offset)
            : field(field), type(type_layout), offset(offset) {}
    };

    struct StructLayout {
        ebm::StatementRef struct_ref;
        ebm::TypeRef type;
        std::vector<FieldLayout> fields;
        size_t size = 0;

        void add_field(ebm::StatementRef related, TypeLayout type) {
            fields.emplace_back(related, type, size);
            size += type.size;
        }
    };

    struct ArrayLayout {
        TypeLayout element_type;
        size_t length;
        size_t size;
    };

    struct VectorLayout {
        TypeLayout element_type;
    };

    struct UnionLayout {
        std::vector<TypeLayout> variants;
        size_t size;
    };

    struct TypeLayoutContext {
       private:
        std::unordered_map<ebm::TypeRef, TypeLayout> type_layouts;
        std::unordered_map<ebm::StatementRef, StructLayout> struct_layouts;
        std::unordered_map<ebm::TypeRef, ArrayLayout> array_layouts;
        std::unordered_map<std::pair<size_t, /*element_type*/ ebm::TypeRef>, ebm::TypeRef> array_layout_reverse_lookup;
        std::unordered_map<ebm::TypeRef, VectorLayout> vector_layouts;
        std::unordered_map<ebm::TypeRef, UnionLayout> union_layouts;
        ebm::TypeRef u8_type;

        friend struct LayoutAccess;

       public:
        void set_u8_type(ebm::TypeRef type) {
            u8_type = type;
        }

        void add_type_layout(ebm::TypeRef type, TypeLayout layout) {
            type_layouts[type] = layout;
        }

        void add_struct_layout(ebm::StatementRef struct_ref, StructLayout layout) {
            struct_layouts[struct_ref] = std::move(layout);
        }

        void add_array_layout(ebm::TypeRef type, ArrayLayout layout) {
            array_layouts[type] = layout;
            array_layout_reverse_lookup[std::make_pair(layout.length, layout.element_type.type)] = type;
        }

        void add_vector_layout(ebm::TypeRef type, VectorLayout layout) {
            vector_layouts[type] = layout;
        }

        void add_union_layout(ebm::TypeRef type, UnionLayout layout) {
            union_layouts[type] = std::move(layout);
        }

        TypeLayout* get_type_layout(ebm::TypeRef type) {
            auto it = type_layouts.find(type);
            if (it != type_layouts.end()) {
                return &it->second;
            }
            return nullptr;
        }
    };

    inline expected<TypeLayout> analyze_layout(InitialContext& ctx, ebm::TypeRef type) {
        auto& context = *ctx.config().layout_context;
        MAYBE(type_stmt, ctx.get(type));
        if (auto found = context.get_type_layout(type)) {
            return *found;
        }
        if (auto size = type_stmt.body.size()) {
            auto bit = size->value();  // currently, align up to byte, so size must be multiple of 8
            bit = (bit + 7) & ~7;
            TypeLayout layout(type, bit / 8);
            context.add_type_layout(type, layout);
            if (type_stmt.body.kind == ebm::TypeKind::UINT && size->value() == 8) {
                context.set_u8_type(type);
            }
            return layout;
        }
        if (auto base_type = type_stmt.body.base_type()) {  // enum
            MAYBE(base_layout, analyze_layout(ctx, *base_type));
            context.add_type_layout(type, base_layout);
            return base_layout;
        }
        if (auto length = type_stmt.body.length()) {
            MAYBE(element_type, type_stmt.body.element_type());
            MAYBE(element_layout, analyze_layout(ctx, element_type));
            auto bit = element_layout.size * 8 * length->value();  // currently, align up to byte, so size must be multiple of 8
            bit = (bit + 7) & ~7;
            TypeLayout layout(type, bit / 8);
            context.add_type_layout(type, layout);
            context.add_array_layout(
                type,
                ArrayLayout{
                    .element_type = element_layout,
                    .length = length->value(),
                    .size = layout.size,
                });
            return layout;
        }
        if (auto elem = type_stmt.body.element_type()) {  // vector
            MAYBE(element_layout, analyze_layout(ctx, *elem));
            TypeLayout pointer_layout(type, sizeof(std::uint8_t*));
            context.add_type_layout(type, pointer_layout);
            context.add_vector_layout(
                type,
                VectorLayout{
                    .element_type = element_layout,
                });
            return pointer_layout;
        }
        if (auto id = type_stmt.body.id()) {
            MAYBE(struct_stmt, ctx.get(*id));
            if (struct_stmt.body.kind != ebm::StatementKind::STRUCT_DECL) {
                return ebmgen::unexpect_error("type id does not refer to a struct");
            }
            StructLayout layout;
            layout.type = type;
            layout.struct_ref = from_weak(*id);
            MAYBE(field_list, struct_stmt.body.struct_decl());
            auto res = handle_fields(ctx, field_list.fields, true, [&](ebm::StatementRef field_ref, const ebm::Statement& field) -> ebmgen::expected<void> {
                MAYBE(field_body, field.body.field_decl());
                MAYBE(field_type_layout, analyze_layout(ctx, field_body.field_type));
                layout.add_field(field_ref, field_type_layout);
                return {};
            });
            if (!res) {
                return ebmgen::unexpect_error(std::move(res.error()));
            }
            context.add_struct_layout(from_weak(*id), std::move(layout));
            context.add_type_layout(type, TypeLayout(type, layout.size));
            return TypeLayout(type, layout.size);
        }
        if (auto variants = type_stmt.body.variant_desc()) {
            std::vector<TypeLayout> variant_layouts;
            size_t max_size = 0;
            for (auto& variant_type : variants->members.container) {
                MAYBE(variant_layout, analyze_layout(ctx, variant_type));
                if (variant_layout.size > max_size) {
                    max_size = variant_layout.size;
                }
                variant_layouts.push_back(variant_layout);
            }
            // currently, align up to byte, so size must be multiple of 8
            size_t bit = max_size * 8;
            bit = (bit + 7) & ~7;
            TypeLayout layout(type, bit / 8);
            context.add_type_layout(type, layout);
            context.add_union_layout(type, UnionLayout{
                                               .variants = std::move(variant_layouts),
                                               .size = layout.size,
                                           });
            return layout;
        }
        return ebmgen::unexpect_error("unable to analyze layout for type");
    }

    struct ObjectRef {
        ebm::TypeRef type;
        futils::view::wvec raw_object;

        constexpr ObjectRef() = default;

        constexpr ObjectRef(ebm::TypeRef type, futils::view::wvec raw_object)
            : type(type), raw_object(raw_object) {}
    };

    struct LayoutAccess {
       private:
        InitialContext& ctx;

        auto& context() {
            return *ctx.config().layout_context;
        }

       public:
        LayoutAccess(InitialContext& ctx)
            : ctx(ctx) {}
        expected<TypeLayout> get_type_layout(ebm::TypeRef type) {
            auto layout = context().get_type_layout(type);
            if (!layout) {
                return ebmgen::unexpect_error("type layout not found");
            }
            return *layout;
        }

        expected<TypeLayout> get_struct_layout(ebm::StatementRef struct_ref) {
            auto layout = context().struct_layouts.find(struct_ref);
            if (layout == context().struct_layouts.end()) {
                return ebmgen::unexpect_error("struct layout not found");
            }
            return TypeLayout(layout->second.type, layout->second.size);
        }

        StructLayout* get_struct_layout_detail(ebm::TypeRef type) {
            if (auto type_stmt = ctx.get_field<"body.id">(type)) {
                return get_struct_layout_detail(from_weak(*type_stmt));
            }
            return nullptr;
        }

        StructLayout* get_struct_layout_detail(ebm::StatementRef struct_ref) {
            auto layout = context().struct_layouts.find(struct_ref);
            if (layout == context().struct_layouts.end()) {
                return nullptr;
            }
            return &layout->second;
        }

        expected<ObjectRef> read_member(ObjectRef target, ebm::StatementRef field_ref) {
            MAYBE(field_parent, ctx.get_field<"field_decl">(field_ref));
            auto found = context().struct_layouts.find(from_weak(field_parent.parent_struct));
            if (found == context().struct_layouts.end()) {
                return ebmgen::unexpect_error("struct layout not found for field");
            }
            for (auto& field_layout : found->second.fields) {
                if (field_layout.field == field_ref) {
                    auto got = target.raw_object.substr(field_layout.offset, field_layout.type.size);
                    if (got.size() != field_layout.type.size) {
                        return ebmgen::unexpect_error("field size does not match layout size");
                    }
                    return ObjectRef(field_layout.type.type, got);
                }
            }
            return ebmgen::unexpect_error("field not found in struct layout");
        }

        expected<TypeLayout> get_u8_n_array(size_t length) {
            auto it = context().array_layout_reverse_lookup.find(std::make_pair(length, context().u8_type));
            if (it == context().array_layout_reverse_lookup.end()) {
                return ebmgen::unexpect_error("u8 array layout not found for length {}", length);
            }
            auto array_type = it->second;
            auto layout = context().get_type_layout(array_type);
            if (!layout) {
                return ebmgen::unexpect_error("type layout not found for u8 array of length {}", length);
            }
            return *layout;
        }

        expected<TypeLayout> get_array_element_type(ebm::TypeRef array_type) {
            auto layout = context().array_layouts.find(array_type);
            if (layout == context().array_layouts.end()) {
                return ebmgen::unexpect_error("array layout not found");
            }
            return layout->second.element_type;
        }
    };
}  // namespace ebm2rmw
