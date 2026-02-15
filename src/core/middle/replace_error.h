/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>
#include <core/ast/tool/extract_config.h>

namespace brgen::middle {
    inline result<void> replace_explicit_error(const std::shared_ptr<ast::Node>& node) {
        auto recursive = [&](auto&& f, auto&& node) -> void {
            auto one_element = [&](auto it) {
                f(f, *it);
                auto c = ast::tool::extract_config(*it, ast::tool::ExtractMode::call, true);
                if (!c) {
                    return;
                }
                if (c->name != "error") {
                    return;
                }
                if (c->arguments.size() < 1) {
                    error((*it)->loc, "error() requires at least one argument").report();
                }
                if (c->arguments[0]->node_type != ast::NodeType::str_literal) {
                    error((*it)->loc, "error() requires a string literal as the first argument").report();
                }
                auto str = ast::cast_to<ast::StrLiteral>(c->arguments[0]);
                auto ca = ast::cast_to<ast::Call>(*it);
                ast::as<ast::Ident>(ca->callee)->usage = ast::IdentUsage::reference_builtin_fn;
                *it = std::make_shared<ast::ExplicitError>(std::move(ca), std::move(str));
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
                for (auto& l : b->struct_type->fields) {  // for nested decl
                    f(f, l);
                }
                return;
            }
            if (auto s = ast::as<ast::ScopedStatement>(node)) {
                one_element(&s->statement);
                for (auto& l : s->struct_type->fields) {  // for nested decl
                    f(f, l);
                }
                return;
            }

            ast::traverse(node, [&](auto&& v) {
                f(f, v);
            });
        };
        try {
            recursive(recursive, node);
        } catch (LocationError err) {
            return unexpect(std::move(err));
        }
        return {};
    }
}  // namespace brgen::middle
