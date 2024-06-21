/*license*/
#pragma once
#include "../ast.h"
#include "../traverse.h"

namespace brgen::ast::tool {
    inline std::optional<std::pair<std::shared_ptr<ast::Ident>, bool>> lookup_base(const std::shared_ptr<ast::Ident>& i) {
        if (!i) {
            return std::nullopt;
        }
        auto ref = i;
        bool via_member_access = false;
        for (;;) {
            auto base = ref->base.lock();
            if (!base) {
                return std::nullopt;
            }
            if (base->node_type == NodeType::member_access) {
                auto access = ast::cast_to<ast::MemberAccess>(base);
                auto target = access->base.lock();
                if (!target) {
                    return std::nullopt;
                }
                ref = std::move(target);
                via_member_access = true;
            }
            else if (base->node_type == NodeType::ident) {
                ref = ast::cast_to<ast::Ident>(base);
            }
            else {
                return std::pair{ref, via_member_access};
            }
        }
    }

    inline void extract_ident(const std::shared_ptr<Expr>& expr, auto&& add_ident) {
        if (auto ident = ast::as<ast::Ident>(expr)) {
            add_ident(ast::cast_to<ast::Ident>(expr));
            return;
        }
        auto lookup = [&](auto&& f, const std::shared_ptr<Node>& v) -> void {
            if (ast::as<ast::Type>(v)) {
                return;
            }
            if (ast::as<ast::Ident>(v)) {
                add_ident(ast::cast_to<ast::Ident>(v));
                return;
            }
            traverse(v, [&](auto&& v) {
                f(f, v);
            });
        };
        traverse(expr, [&](auto&& v) -> void {
            lookup(lookup, v);
        });
    }

    inline std::vector<std::shared_ptr<ast::Binary>> collect_defined_ident(const std::shared_ptr<ast::Expr>& expr) {
        std::vector<std::shared_ptr<ast::Binary>> defs;
        auto f = [&](auto&& f, auto&& v) -> void {
            ast::traverse(v, [&](auto&& v) -> void {
                f(f, v);
            });
            if (auto ident = ast::as<ast::Ident>(v)) {
                auto base = ast::as<ast::Ident>(ident->base.lock());
                if (!base) {
                    return;
                }
                auto base_base = base->base.lock();
                if (auto bin = ast::as<ast::Binary>(base_base)) {
                    if (bin->op == BinaryOp::define_assign ||
                        bin->op == BinaryOp::const_assign) {
                        defs.push_back(ast::cast_to<ast::Binary>(base_base));
                    }
                }
            }
        };
        ast::traverse(expr, [&](auto&& v) -> void {
            f(f, v);
        });
        return defs;
    }

    inline std::shared_ptr<Format> belong_format(const std::shared_ptr<ast::Ident>& typ) {
        auto base = lookup_base(typ);
        if (!base) {
            return nullptr;
        }
        if (Field* f = ast::as<Field>(base->first->base.lock())) {
            auto fmt = f->belong.lock();
            if (!ast::as<Format>(fmt)) {
                return nullptr;
            }
            return ast::cast_to<Format>(fmt);
        }
        return nullptr;
    }

    bool is_on_named_struct(const std::shared_ptr<ast::Field>& field) {
        if (!field) {
            return false;
        }
        if (auto fmt = ast::as<ast::Format>(field->belong.lock())) {
            return field->belong_struct.lock() == fmt->body->struct_type;
        }
        if (auto state = ast::as<ast::State>(field->belong.lock())) {
            return field->belong_struct.lock() == state->body->struct_type;
        }
        return false;
    }
}  // namespace brgen::ast::tool