/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <set>
#include <core/ast/tool/eval.h>

namespace brgen::middle {

    struct TypeAttribute {
        void recursive_reference(const std::shared_ptr<ast::Node>& node) {
            std::set<ast::StructType*> typ;
            std::set<ast::Type*> tracked;
            auto traverse_fn = [&](auto&& f, const std::shared_ptr<ast::Node>& n) -> void {
                if (auto t = ast::as<ast::StructType>(n); t) {
                    if (tracked.find(t) != tracked.end()) {
                        return;
                    }
                    if (t->recursive) {
                        return;
                    }
                    if (typ.find(t) != typ.end()) {
                        t->recursive = true;
                    }
                    typ.insert(t);
                    ast::traverse(n, [&](auto&& n) {
                        f(f, n);
                    });
                    typ.erase(t);
                    tracked.insert(t);
                }
                if (auto t = ast::as<ast::IdentType>(n)) {
                    auto c = t->base.lock();
                    if (c) {
                        f(f, c);
                    }
                }
                ast::traverse(n, [&](auto&& n) {
                    f(f, n);
                });
            };
            traverse_fn(traverse_fn, node);
        }

        void int_type_detection(const std::shared_ptr<ast::Node>& node) {
            std::set<ast::Type*> tracked;
            auto trv = [&](auto&& f, const std::shared_ptr<ast::Node>& n) -> void {
                if (auto ty = ast::as<ast::Type>(n)) {
                    if (tracked.find(ty) != tracked.end()) {
                        return;  // already detected
                    }
                    tracked.insert(ty);
                }
                if (auto t = ast::as<ast::StructType>(n); t && !t->recursive) {
                    if (t->is_int_set || t->recursive) {
                        return;  // already detected
                    }
                    ast::traverse(n, [&](auto&& n) {
                        f(f, n);
                    });
                    bool all_int = true;
                    bool has_field = false;
                    for (auto& fields : t->fields) {
                        if (auto field = ast::as<ast::Field>(fields); field) {
                            has_field = true;
                            if (!field->field_type->is_int_set) {
                                all_int = false;
                                break;
                            }
                        }
                    }
                    t->is_int_set = has_field && all_int;
                    return;
                }
                if (auto t = ast::as<ast::IdentType>(n)) {
                    auto c = t->base.lock();
                    if (c) {
                        f(f, c);
                        t->is_int_set = c->is_int_set;
                    }
                }
                ast::traverse(n, [&](auto&& n) {
                    f(f, n);
                });
                if (auto t = ast::as<ast::IntType>(n); t) {
                    t->is_int_set = true;
                }
                if (auto a = ast::as<ast::ArrayType>(n); a) {
                    if (a->length && a->length->constant_level == ast::ConstantLevel::constant &&
                        a->length->node_type != ast::NodeType::range &&  // if b: [..]u8 style, b is variable length array, so not int set
                        a->base_type->is_int_set) {
                        a->is_int_set = true;
                    }
                }
                if (auto t = ast::as<ast::StrLiteralType>(n)) {
                    // magic number type
                    t->is_int_set = true;
                }
                if (auto u = ast::as<ast::StructUnionType>(n)) {
                    u->is_int_set = true;
                    for (auto& f : u->structs) {
                        if (!f->is_int_set) {
                            u->is_int_set = false;
                        }
                    }
                }
                if (auto u = ast::as<ast::UnionType>(n)) {
                    u->is_int_set = true;
                    for (auto& c : u->candidates) {
                        if (auto f = ast::as<ast::Field>(c->field.lock()); f) {
                            if (!f->field_type->is_int_set) {
                                u->is_int_set = false;
                            }
                        }
                    }
                }
                if (auto c = ast::as<ast::EnumType>(n)) {
                    // TODO(on-keyday): string enum type is not int set
                    c->is_int_set = true;
                }
            };
            trv(trv, node);
        }

        void bit_alignment(const std::shared_ptr<ast::Node>& node) {
            std::set<ast::Type*> tracked;
            auto trv = [&](auto&& f, const std::shared_ptr<ast::Node>& n) -> void {
                if (auto ty = ast::as<ast::Type>(n); ty) {
                    if (tracked.find(ty) != tracked.end()) {
                        return;  // already detected
                    }
                    tracked.insert(ty);
                }
                if (auto t = ast::as<ast::StructType>(n); t && !t->recursive) {
                    if (t->bit_alignment != ast::BitAlignment::not_target) {
                        return;  // already detected
                    }
                    ast::traverse(n, [&](auto&& n) {
                        f(f, n);
                    });
                    bool has_field = false;
                    ast::BitAlignment alignment = ast::BitAlignment::byte_aligned;
                    size_t bit_size = -1;
                    for (auto& fields : t->fields) {
                        if (auto field = ast::as<ast::Field>(fields); field) {
                            // TODO(on-keyday): bit alignment and size of field is affected by field argument
                            if (field->field_type->bit_alignment == ast::BitAlignment::not_target) {
                                field->bit_alignment = ast::BitAlignment::not_target;
                                continue;
                            }
                            has_field = true;
                            if (field->field_type->bit_alignment == ast::BitAlignment::not_decidable) {
                                alignment = ast::BitAlignment::not_decidable;
                            }
                            if (alignment == ast::BitAlignment::not_decidable) {
                                field->bit_alignment = ast::BitAlignment::not_decidable;
                                continue;
                            }
                            auto new_align = (int(alignment) - int(ast::BitAlignment::byte_aligned)) + (int(field->field_type->bit_alignment) - int(ast::BitAlignment::byte_aligned));
                            new_align %= utils::bit_per_byte;
                            alignment = ast::BitAlignment(new_align + int(ast::BitAlignment::byte_aligned));
                            field->bit_alignment = alignment;
                            if (bit_size == -1) {
                                bit_size = field->field_type->bit_size;
                            }
                            else if (bit_size != 0) {
                                if (field->field_type->bit_size == 0) {
                                    bit_size = 0;
                                }
                                else {
                                    bit_size += field->field_type->bit_size;
                                }
                            }
                        }
                    }
                    if (has_field) {
                        t->bit_alignment = alignment;
                        t->bit_size = bit_size == -1 ? 0 : bit_size;
                    }
                    return;
                }
                if (auto t = ast::as<ast::IdentType>(n)) {
                    auto c = t->base.lock();
                    if (c) {
                        f(f, c);
                        t->bit_alignment = c->bit_alignment;
                        t->bit_size = c->bit_size;
                    }
                }
                ast::traverse(n, [&](auto&& n) {
                    f(f, n);
                });
                if (auto t = ast::as<ast::IntType>(n); t) {
                    auto align = (t->bit_size % utils::bit_per_byte);
                    t->bit_alignment = ast::BitAlignment(align + int(ast::BitAlignment::byte_aligned));
                }
                if (auto a = ast::as<ast::ArrayType>(n); a) {
                    // state
                    // 1. a->has_const_length == true , a->length_value != 0, a->base_type->bit_size != 0 -> fixed length array (size known)
                    // 2. a->has_const_length == true , a->length_value != 0, a->base_type->bit_size == 0 -> fixed length array (size unknown)
                    // 3. a->has_const_length == true , a->length_value == 0, a->base_type->bit_size != 0 -> zero length array (size known==0)
                    // 4. a->has_const_length == true , a->length_value == 0, a->base_type->bit_size == 0 -> zero length array (size known==0)
                    // 5. a->has_const_length == false, a->length_value is any, a->base_type->bit_size != 0 -> variable length array (size unknown)
                    // 6. a->has_const_length == false, a->length_value is any, a->base_type->bit_size == 0 -> variable length array (size unknown)

                    // if a->length->constant_level == constant,
                    // and eval result has integer type, then a->length_value is set and a->has_const_length is true
                    if (a->length && a->length->constant_level == ast::ConstantLevel::constant) {
                        ast::tool::Evaluator eval;
                        eval.ident_mode = ast::tool::EvalIdentMode::resolve_ident;
                        if (auto val = eval.eval_as<ast::tool::EResultType::integer>(a->length)) {
                            // case 1 or 2
                            a->length_value = val->get<ast::tool::EResultType::integer>();
                            a->has_const_length = true;
                        }
                    }
                    // determine bit size
                    if (a->length && a->has_const_length) {
                        a->bit_size = a->length_value * a->base_type->bit_size;
                    }
                    else {
                        a->bit_size = 0;  // variable length array
                    }
                    if (a->base_type->bit_alignment == ast::BitAlignment::byte_aligned ||
                        a->base_type->bit_alignment == ast::BitAlignment::not_target ||
                        a->base_type->bit_alignment == ast::BitAlignment::not_decidable) {
                        a->bit_alignment = a->base_type->bit_alignment;
                    }
                    else {
                        // TODO(on-keyday): bit alignment of fixed length array non byte aligned can also be decided
                        a->bit_alignment = ast::BitAlignment::not_decidable;
                    }
                }
                if (auto t = ast::as<ast::StrLiteralType>(n)) {
                    t->bit_alignment = ast::BitAlignment::byte_aligned;
                }
                if (auto u = ast::as<ast::StructUnionType>(n)) {
                    ast::BitAlignment alignment = ast::BitAlignment::not_target;
                    size_t bit_size = -1;
                    for (auto& f : u->structs) {
                        if (f->bit_alignment == ast::BitAlignment::not_target) {
                            continue;
                        }
                        if (f->bit_alignment == ast::BitAlignment::not_decidable) {
                            bit_size = 0;
                            alignment = ast::BitAlignment::not_decidable;
                            break;
                        }
                        if (alignment == ast::BitAlignment::not_target) {
                            alignment = f->bit_alignment;
                        }
                        if (bit_size == -1) {
                            bit_size = f->bit_size;
                        }
                        else if (bit_size != 0) {
                            if (bit_size != f->bit_size) {
                                bit_size = 0;
                            }
                        }
                        if (alignment != f->bit_alignment) {
                            alignment = ast::BitAlignment::not_decidable;
                            bit_size = 0;
                            break;
                        }
                    }
                    u->bit_size = bit_size;
                    u->bit_alignment = alignment;
                }
                if (auto u = ast::as<ast::UnionType>(n)) {
                    // not a target
                    u->bit_alignment = ast::BitAlignment::not_target;
                    size_t size = -1;
                    // estimate bit size
                    for (auto& c : u->candidates) {
                        auto f = c->field.lock();
                        if (!f) {
                            continue;
                        }
                        if (size == -1) {
                            size = f->field_type->bit_size;
                        }
                        else if (size != 0) {
                            if (size != f->field_type->bit_size) {
                                size = 0;
                                break;
                            }
                        }
                    }
                    if (size == -1) {
                        size = 0;
                    }
                    u->bit_size = size;
                }
                if (auto e = ast::as<ast::EnumType>(n)) {
                    auto b = e->base.lock();
                    if (b && b->base_type) {
                        e->bit_alignment = b->base_type->bit_alignment;
                        e->bit_size = b->base_type->bit_size;
                    }
                    else {
                        // bit alignment of enum which is not determined base type is not decidable
                        e->bit_alignment = ast::BitAlignment::not_decidable;
                        // unknown bit size
                        e->bit_size = 0;
                    }
                }
            };
            trv(trv, node);
        }
    };
}  // namespace brgen::middle
