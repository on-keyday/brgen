/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <set>
#include <core/ast/tool/eval.h>
#include <core/ast/node/ast_enum.h>

namespace brgen::middle {

    struct TypeAttribute {
        void mark_recursive_reference(const std::shared_ptr<ast::Node>& node) {
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

        void detect_non_dynamic_type(const std::shared_ptr<ast::Node>& node) {
            std::set<ast::Type*> tracked;
            auto trv = [&](auto&& f, const std::shared_ptr<ast::Node>& n) -> void {
                if (auto ty = ast::as<ast::Type>(n)) {
                    if (tracked.find(ty) != tracked.end()) {
                        return;  // already detected
                    }
                    tracked.insert(ty);
                }
                if (auto t = ast::as<ast::StructType>(n); t && !t->recursive) {
                    if (t->non_dynamic_allocation || t->recursive) {
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
                            if (!field->field_type->non_dynamic_allocation) {
                                all_int = false;
                                break;
                            }
                        }
                    }
                    t->non_dynamic_allocation = !has_field || all_int;
                    return;
                }
                if (auto t = ast::as<ast::IdentType>(n)) {
                    auto c = t->base.lock();
                    if (c) {
                        f(f, c);
                        t->non_dynamic_allocation = c->non_dynamic_allocation;
                    }
                }
                ast::traverse(n, [&](auto&& n) {
                    f(f, n);
                });
                if (auto t = ast::as<ast::IntType>(n); t) {
                    t->non_dynamic_allocation = true;
                }
                if (auto a = ast::as<ast::ArrayType>(n); a) {
                    if (a->length && a->length->constant_level == ast::ConstantLevel::constant &&
                        a->length->node_type != ast::NodeType::range &&  // if b: [..]u8 style, b is variable length array, so not int set
                        a->element_type->non_dynamic_allocation) {
                        a->non_dynamic_allocation = true;
                    }
                }
                if (auto t = ast::as<ast::StrLiteralType>(n)) {
                    // magic number type
                    t->non_dynamic_allocation = true;
                }
                if (auto u = ast::as<ast::StructUnionType>(n)) {
                    u->non_dynamic_allocation = true;
                    for (auto& f : u->structs) {
                        if (!f->non_dynamic_allocation) {
                            u->non_dynamic_allocation = false;
                        }
                    }
                }
                if (auto u = ast::as<ast::UnionType>(n)) {
                    u->non_dynamic_allocation = true;
                    for (auto& c : u->candidates) {
                        if (auto f = ast::as<ast::Field>(c->field.lock()); f) {
                            if (!f->field_type->non_dynamic_allocation) {
                                u->non_dynamic_allocation = false;
                            }
                        }
                    }
                }
                if (auto c = ast::as<ast::EnumType>(n)) {
                    // TODO(on-keyday): string enum type is not int set?
                    c->non_dynamic_allocation = true;
                }
                if (auto c = ast::as<ast::FloatType>(n)) {
                    c->non_dynamic_allocation = true;
                }
            };
            trv(trv, node);
        }

       private:
        bool ignore_on_follow_analysis(ast::Field* field) {
            if (field->arguments) {
                if (field->arguments->peek_value) {
                    return *field->arguments->peek_value;
                }
                if (field->arguments->sub_byte_begin) {
                    return true;
                }
            }
            return false;
        }

        void analyze_forward_bit_alignment_and_size(ast::StructType* t) {
            ast::BitAlignment alignment = ast::BitAlignment::byte_aligned;
            std::optional<size_t> bit_size = 0;
            size_t offset = 0;
            ast::Field* prev_field = nullptr;
            t->bit_size = 0;
            auto calc_new_align = [&](ast::BitAlignment field_algin) {
                auto new_align = (int(alignment) - int(ast::BitAlignment::byte_aligned)) + (int(field_algin) - int(ast::BitAlignment::byte_aligned));
                return ast::BitAlignment(new_align % futils::bit_per_byte);
            };
            for (auto& fields : t->fields) {
                if (auto field = ast::as<ast::Field>(fields); field) {
                    if (field->field_type->bit_alignment == ast::BitAlignment::not_target) {
                        field->bit_alignment = ast::BitAlignment::not_target;
                        field->eventual_bit_alignment = ast::BitAlignment::not_target;
                        continue;
                    }

                    if (ignore_on_follow_analysis(field)) {
                        if (field->field_type->bit_alignment == ast::BitAlignment::not_decidable) {
                            field->bit_alignment = ast::BitAlignment::not_decidable;
                        }
                        else {
                            field->bit_alignment = calc_new_align(field->field_type->bit_alignment);
                        }
                        field->offset_bit = bit_size;
                        field->offset_recent = offset;
                        field->eventual_bit_alignment = field->bit_alignment;
                        continue;
                    }

                    // calculate follow
                    if (prev_field) {
                        if (ast::as<ast::StrLiteralType>(field->field_type)) {
                            prev_field->follow = ast::Follow::constant;
                        }
                        else if (field->field_type->bit_size) {
                            prev_field->follow = ast::Follow::fixed;
                        }
                        else {
                            prev_field->follow = ast::Follow::normal;
                        }
                    }
                    prev_field = field;

                    // calculate offset
                    field->offset_bit = bit_size;
                    field->offset_recent = offset;
                    if (field->field_type->bit_size) {
                        offset += *field->field_type->bit_size;
                    }
                    else {
                        offset = 0;
                    }

                    // calculate bit size
                    if (bit_size == 0) {
                        bit_size = field->field_type->bit_size;
                    }
                    else if (bit_size) {
                        if (!field->field_type->bit_size) {
                            t->fixed_header_size = *bit_size;
                            bit_size = std::nullopt;
                        }
                        else {
                            *bit_size += *field->field_type->bit_size;
                        }
                    }

                    // calculate bit alignment
                    // TODO(on-keyday): bit alignment and size of field is affected by field argument
                    if (field->field_type->bit_alignment == ast::BitAlignment::not_decidable) {
                        alignment = ast::BitAlignment::not_decidable;
                    }
                    if (alignment == ast::BitAlignment::not_decidable) {
                        field->bit_alignment = ast::BitAlignment::not_decidable;
                        continue;
                    }
                    alignment = calc_new_align(field->field_type->bit_alignment);
                    field->bit_alignment = alignment;
                }
            }
            if (prev_field) {
                prev_field->follow = ast::Follow::end;
            }
            t->bit_alignment = alignment;
            t->bit_size = bit_size;
            if (bit_size) {
                t->fixed_header_size = *bit_size;
            }
        }

        void analyze_backward_size_and_follow(ast::StructType* t) {
            std::optional<size_t> tail_bit_size = 0;
            size_t tail_offset = 0;
            ast::Follow current = ast::Follow::unknown;
            std::optional<ast::BitAlignment> eventual_alignment;
            for (auto it = t->fields.rbegin(); it != t->fields.rend(); it++) {
                if (auto field = ast::as<ast::Field>(*it); field) {
                    if (ignore_on_follow_analysis(field)) {
                        field->tail_offset_bit = tail_bit_size;
                        field->tail_offset_recent = tail_offset;
                        field->eventual_follow = ast::Follow::unknown;
                        continue;
                    }

                    // calculate eventual follow
                    if (field->follow == ast::Follow::unknown) {
                        continue;
                    }
                    if (field->follow == ast::Follow::end) {
                        current = field->follow;
                    }
                    else if (field->follow == ast::Follow::normal) {
                        current = field->follow;
                    }
                    else if (field->follow == ast::Follow::constant) {
                        current = field->follow;
                    }
                    assert(current != ast::Follow::unknown);
                    field->eventual_follow = current;
                    if (eventual_alignment) {
                        if (field->bit_alignment == ast::BitAlignment::byte_aligned) {
                            field->eventual_bit_alignment = ast::BitAlignment::byte_aligned;
                        }
                        else {
                            field->eventual_bit_alignment = *eventual_alignment;
                        }
                    }
                    else {
                        field->eventual_bit_alignment = field->bit_alignment;
                        eventual_alignment = field->bit_alignment;
                    }

                    // calculate tail offset
                    field->tail_offset_bit = tail_bit_size;
                    field->tail_offset_recent = tail_offset;
                    if (field->field_type->bit_size) {
                        tail_offset += *field->field_type->bit_size;
                    }
                    else {
                        tail_offset = 0;
                    }

                    // calculate tail bit size
                    if (tail_bit_size == 0) {
                        tail_bit_size = field->field_type->bit_size;
                    }
                    else if (tail_bit_size) {
                        if (!field->field_type->bit_size) {
                            t->fixed_tail_size = *tail_bit_size;
                            tail_bit_size = std::nullopt;
                        }
                        else {
                            *tail_bit_size += *field->field_type->bit_size;
                        }
                    }
                }
            }
            if (tail_bit_size) {
                t->fixed_tail_size = *tail_bit_size;
            }
        }

        void set_struct_bit_alignment(ast::StructType* t) {
            analyze_forward_bit_alignment_and_size(t);
            analyze_backward_size_and_follow(t);
        }

        void set_struct_union_bit_alignment(ast::StructUnionType* u) {
            ast::BitAlignment alignment = ast::BitAlignment::not_target;
            std::optional<size_t> bit_size = 0;
            for (auto& f : u->structs) {
                if (f->bit_alignment == ast::BitAlignment::not_target) {
                    continue;
                }
                if (f->bit_alignment == ast::BitAlignment::not_decidable) {
                    bit_size = std::nullopt;
                    alignment = ast::BitAlignment::not_decidable;
                    break;
                }
                if (alignment == ast::BitAlignment::not_target) {
                    alignment = f->bit_alignment;
                }
                if (bit_size == 0) {
                    bit_size = f->bit_size;
                }
                else if (bit_size) {
                    if (bit_size != f->bit_size) {
                        bit_size = std::nullopt;
                    }
                }
                // if f->bit_size == 0, any of alignment is suitable for union
                if (alignment != f->bit_alignment && f->bit_size != 0) {
                    alignment = ast::BitAlignment::not_decidable;
                    bit_size = std::nullopt;
                    break;
                }
            }
            if (u->exhaustive && bit_size) {
                u->bit_size = bit_size;
            }
            u->bit_alignment = alignment;
        }

       public:
        void analyze_bit_size_and_alignment(const std::shared_ptr<ast::Node>& node) {
            std::set<ast::Type*> tracked;
            auto trv = [&](auto&& f, const std::shared_ptr<ast::Node>& n) -> void {
                if (auto ty = ast::as<ast::Type>(n); ty) {
                    if (tracked.find(ty) != tracked.end()) {
                        return;  // already detected
                    }
                    tracked.insert(ty);
                }
                if (auto t = ast::as<ast::StructType>(n); t) {
                    if (t->bit_alignment != ast::BitAlignment::not_target) {
                        return;  // already detected
                    }
                    ast::traverse(n, [&](auto&& n) {
                        f(f, n);
                    });
                    set_struct_bit_alignment(t);
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
                    auto align = (*t->bit_size % futils::bit_per_byte);
                    t->bit_alignment = ast::BitAlignment(align + int(ast::BitAlignment::byte_aligned));
                    assert(t->bit_alignment != ast::BitAlignment::not_target &&
                           t->bit_alignment != ast::BitAlignment::not_decidable);
                }
                if (auto t = ast::as<ast::FloatType>(n); t) {
                    auto align = (*t->bit_size % futils::bit_per_byte);
                    t->bit_alignment = ast::BitAlignment(align + int(ast::BitAlignment::byte_aligned));
                }
                if (auto a = ast::as<ast::ArrayType>(n); a) {
                    // determine bit size
                    if (a->length_value && a->element_type->bit_size) {
                        a->bit_size = *a->length_value * *a->element_type->bit_size;
                    }
                    else {
                        a->bit_size = std::nullopt;  // variable length array
                    }
                    if (a->element_type->bit_alignment == ast::BitAlignment::byte_aligned ||
                        a->element_type->bit_alignment == ast::BitAlignment::not_target ||
                        a->element_type->bit_alignment == ast::BitAlignment::not_decidable) {
                        a->bit_alignment = a->element_type->bit_alignment;
                    }
                    else {
                        // TODO(on-keyday): bit alignment of fixed length array non byte aligned can also be decided
                        a->bit_alignment = ast::BitAlignment::not_decidable;
                    }
                }
                if (auto t = ast::as<ast::StrLiteralType>(n)) {
                    t->bit_alignment = ast::BitAlignment::byte_aligned;
                    t->bit_size = t->base.lock()->length * futils::bit_per_byte;
                }
                if (auto u = ast::as<ast::StructUnionType>(n)) {
                    set_struct_union_bit_alignment(u);
                    return;
                }
                if (auto u = ast::as<ast::UnionType>(n)) {
                    // not a target
                    u->bit_alignment = ast::BitAlignment::not_target;
                    std::optional<size_t> size = 0;
                    // estimate bit size
                    for (auto& c : u->candidates) {
                        auto f = c->field.lock();
                        if (!f) {
                            continue;
                        }
                        if (size == 0) {
                            size = f->field_type->bit_size;
                        }
                        else if (size) {
                            if (size != f->field_type->bit_size) {
                                size = std::nullopt;
                                break;
                            }
                        }
                    }
                    u->bit_size = size;
                }
                if (auto e = ast::as<ast::EnumType>(n)) {
                    auto b = e->base.lock();

                    if (b && b->base_type) {
                        ast::traverse(b, [&](auto&& n) {
                            f(f, n);
                        });
                        e->bit_alignment = b->base_type->bit_alignment;
                        e->bit_size = b->base_type->bit_size;
                    }
                    else {
                        // bit alignment of enum which is not determined base type is not decidable
                        e->bit_alignment = ast::BitAlignment::not_decidable;
                        // unknown bit size
                        e->bit_size = std::nullopt;
                    }
                }
            };
            trv(trv, node);
        }
    };
}  // namespace brgen::middle
