/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <set>

namespace brgen::middle {

    struct TypeAttribute {
        void recursive_reference(const std::shared_ptr<ast::Node>& node) {
            std::set<ast::StructType*> typ;
            auto traverse_fn = [&](auto&& f, const std::shared_ptr<ast::Node>& n) -> void {
                if (auto t = ast::as<ast::StructType>(n); t) {
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
                if (auto t = ast::as<ast::StructType>(n); t && !t->recursive) {
                    if (tracked.find(t) != tracked.end()) {
                        return;  // already detected
                    }
                    if (t->is_int_set) {
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
                    tracked.insert(t);
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
            };
            trv(trv, node);
        }
    };
}  // namespace brgen::middle
