/*license*/
#pragma once
#include "layout.h"

namespace brgen::vm2 {
    std::shared_ptr<Layout> TypeContext::compute_layout(const std::shared_ptr<ast::Type>& typ) {
        if (auto it = this->layouts.find(typ); it != this->layouts.end()) {
            return it->second;  // layout already computed
        }
        if (auto ty = ast::as<ast::IntType>(typ)) {
            auto size = *ty->bit_size / 8 + (*ty->bit_size % 8 != 0);
            auto prim = std::make_shared<Primitive>(size, size, typ);
            this->layouts[typ] = prim;
            return prim;
        }
        if (auto ty = ast::as<ast::FloatType>(typ)) {
            auto size = *ty->bit_size / 8 + (*ty->bit_size % 8 != 0);
            auto prim = std::make_shared<Primitive>(size, size, typ);
            this->layouts[typ] = prim;
            return prim;
        }
        if (auto ty = ast::as<ast::IdentType>(typ)) {
            auto base = compute_layout(ty->base.lock());
            if (!base) {
                return nullptr;
            }
            auto inf = std::make_shared<Derived>(base, typ);
            this->layouts[typ] = inf;
            return inf;
        }
        if (auto enum_ty = ast::as<ast::EnumType>(typ)) {
            if (!enum_ty->base.lock()->base_type) {
                return nullptr;
            }
            auto computed_layout = compute_layout(enum_ty->base.lock()->base_type);
            if (!computed_layout) {
                return nullptr;
            }
            auto inf = std::make_shared<Derived>(computed_layout, typ);
            this->layouts[typ] = inf;
            return computed_layout;
        }
        if (auto array = ast::as<ast::ArrayType>(typ)) {
            auto element = compute_layout(array->element_type);
            if (!element) {
                return nullptr;
            }
            auto size = array->length_value ? *array->length_value * element->size : 16 /*array tuple (ptr,size) */;
            auto arr = std::make_shared<Array>(size, size, !array->length_value, typ, element);
            this->layouts[typ] = arr;
            return arr;
        }
        if (auto s = ast::as<ast::StructType>(typ)) {
            auto str = std::make_shared<Struct>();
            this->layouts[typ] = str;  // cache the layout
            str->as_field_size = 8;    // pointer to struct
            for (auto& m : s->fields) {
                if (auto field = ast::as<ast::Field>(m)) {
                    if (ast::as<ast::UnionType>(field->field_type)) {
                        continue;
                    }
                    auto name = field->ident ? field->ident->ident : "<anonymous>";
                    auto element = compute_layout(field->field_type);
                    if (!element) {
                        return nullptr;
                    }
                    str->fields.push_back(std::make_shared<Field>(name, element, ast::cast_to<ast::Field>(m), str->size));
                    this->fields[ast::cast_to<ast::Field>(m)] = str->fields.back();
                    str->size += element->as_field_size;
                }
            }
            str->type_ast = typ;
            return str;
        }
        if (auto union_ = ast::as<ast::StructUnionType>(typ)) {
            auto uni = std::make_shared<Union>();
            this->layouts[typ] = uni;
            uni->as_field_size = 8;  // pointer to union
            for (auto& m : union_->structs) {
                auto sub = compute_layout(m);
                if (!sub) {
                    return nullptr;
                }
                uni->elements.push_back(std::static_pointer_cast<Struct>(sub));
                uni->size = std::max(uni->size, sub->size);
            }
            uni->type_ast = typ;
            return uni;
        }
        return nullptr;
    }
}  // namespace brgen::vm2
