/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>

namespace brgen::middle {

    inline bool is_io_related(ast::Expr* a) {
        bool ret = false;
        ast::traverse(a, [&](auto&& f) {
            if (ret) {
                return;
            }
            if (auto t = ast::as<ast::Type>(f)) {
                return;  // continue
            }
            if (ast::as<ast::IOOperation>(f)) {
                ret = true;
                return;  // break
            }
            if (auto expr = ast::as<ast::Expr>(f)) {
                ret = is_io_related(expr);
                return;
            }
        });
        return ret;
    }

    inline void collect_unused_warnings(const std::shared_ptr<ast::Node>& node, LocationError& err_or_warn) {
        if (!node) {
            return;
        }
        auto one_element = [&](auto it) {
            collect_unused_warnings(*it, err_or_warn);
            if (auto bin = ast::as<ast::Binary>(*it); bin && ast::is_assign_op(bin->op)) {
                // nothing to do
            }
            else if (bin || ast::as<ast::Literal>(*it) || ast::as<ast::Unary>(*it) || ast::as<ast::Ident>(*it) ||
                     ast::as<ast::Index>(*it)) {
                err_or_warn.warning((*it)->loc, "unused expression");
            }
        };
        auto each_element = [&](ast::node_list& list) {
            for (auto it = list.begin(); it != list.end(); it++) {
                one_element(it);
            }
        };
        if (auto a = ast::as<ast::Program>(node)) {
            each_element(a->elements);
            return;
        }
        if (auto b = ast::as<ast::IndentBlock>(node)) {
            each_element(b->elements);
            return;
        }
        if (auto s = ast::as<ast::ScopedStatement>(node)) {
            auto base_match = ast::as<ast::MatchBranch>(s->struct_type->base.lock())->belong.lock();
            if (!ast::as<ast::VoidType>(base_match->expr_type)) {
                // at here, match branch is expression that is used as match return value
                return;
            }
            one_element(&s->statement);
            return;
        }
        ast::traverse(node, [&](auto&& f) {
            collect_unused_warnings(f, err_or_warn);
        });
    }

    inline void replace_assert(const std::shared_ptr<ast::Node>& node) {
        if (!node) {
            return;
        }
        auto one_element = [&](auto it) {
            replace_assert(*it);
            if (auto bin = ast::as<ast::Binary>(*it); bin && ast::is_boolean_op(bin->op)) {
                auto a = std::make_shared<ast::Assert>(std::static_pointer_cast<ast::Binary>(*it));
                a->is_io_related = is_io_related(a->cond.get());
                *it = std::move(a);
            }
        };
        auto each_element = [&](ast::node_list& list) {
            for (auto it = list.begin(); it != list.end(); it++) {
                one_element(it);
            }
        };
        if (auto a = ast::as<ast::Program>(node)) {
            each_element(a->elements);
            return;
        }
        if (auto b = ast::as<ast::IndentBlock>(node)) {
            each_element(b->elements);
            return;
        }
        if (auto s = ast::as<ast::ScopedStatement>(node)) {
            auto base_match = ast::as<ast::MatchBranch>(s->struct_type->base.lock())->belong.lock();
            one_element(&s->statement);
            return;
        }
        ast::traverse(node, [&](auto&& f) {
            replace_assert(f);
        });
    }
}  // namespace brgen::middle
