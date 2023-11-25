/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <set>

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
                    if (a->length && a->length->constant_level == ast::ConstantLevel::const_value &&
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
                    for (auto& f : u->fields) {
                        if (!f->is_int_set) {
                            u->is_int_set = false;
                            break;
                        }
                    }
                }
                if (auto u = ast::as<ast::UnionType>(n)) {
                    u->is_int_set = true;
                    for (auto& c : u->candidates) {
                        if (auto f = ast::as<ast::Field>(c->field.lock()); f) {
                            if (!f->field_type->is_int_set) {
                                u->is_int_set = false;
                                break;
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
                    for (auto& fields : t->fields) {
                        if (auto field = ast::as<ast::Field>(fields); field) {
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
                        }
                    }
                    if (has_field) {
                        t->bit_alignment = alignment;
                    }
                    tracked.insert(t);
                    return;
                }
                if (auto t = ast::as<ast::IdentType>(n)) {
                    auto c = t->base.lock();
                    if (c) {
                        f(f, c);
                        t->bit_alignment = c->bit_alignment;
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
                    if (a->length && a->length->constant_level == ast::ConstantLevel::const_value &&
                        a->length->node_type != ast::NodeType::range &&  // if b: [..]u8 style, b is variable length array, so not int set
                        a->base_type->is_int_set) {
                        // TODO(on-keyday): fixed length array bit alignment is decidable
                        //                  even if not byte aligned base type
                        //                  but currently not implemented
                    }
                    if (a->base_type->bit_alignment == ast::BitAlignment::byte_aligned ||
                        a->base_type->bit_alignment == ast::BitAlignment::not_target ||
                        a->base_type->bit_alignment == ast::BitAlignment::not_decidable) {
                        a->bit_alignment = a->base_type->bit_alignment;
                    }
                    else {
                        a->bit_alignment = ast::BitAlignment::not_decidable;
                    }
                }
                if (auto t = ast::as<ast::StrLiteralType>(n)) {
                    t->bit_alignment = ast::BitAlignment::byte_aligned;
                }
                if (auto u = ast::as<ast::StructUnionType>(n)) {
                    ast::BitAlignment alignment = ast::BitAlignment::not_target;
                    for (auto& f : u->fields) {
                        if (f->bit_alignment == ast::BitAlignment::not_target) {
                            continue;
                        }
                        if (f->bit_alignment == ast::BitAlignment::not_decidable) {
                            alignment = ast::BitAlignment::not_decidable;
                            break;
                        }
                        if (alignment == ast::BitAlignment::not_target) {
                            alignment = f->bit_alignment;
                        }
                        if (alignment != f->bit_alignment) {
                            alignment = ast::BitAlignment::not_decidable;
                            break;
                        }
                    }
                    u->bit_alignment = alignment;
                }
                if (auto u = ast::as<ast::UnionType>(n)) {
                    // not a target
                }
                if (auto e = ast::as<ast::EnumType>(n)) {
                    auto b = e->base.lock();
                    if (b && b->base_type) {
                        e->bit_alignment = b->base_type->bit_alignment;
                    }
                    else {
                        // TODO(on-keyday): how to determine bit alignment of enum type?
                        e->bit_alignment = ast::BitAlignment::byte_aligned;
                    }
                }
            };
            trv(trv, node);
        }
    };
}  // namespace brgen::middle
