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
                if (auto t = ast::as<ast::StructType>(n); t) {
                    if (t->recursive) {
                        return;  // not integer
                    }
                    if (t->is_int_set) {
                        return;
                    }
                    ast::traverse(n, [&](auto&& n) {
                        f(f, n);
                    });
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
        }
    };
}  // namespace brgen::middle
