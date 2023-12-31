/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>
#include <core/ast/tool/extract_config.h>

namespace brgen::middle {
    inline void replace_specify_endian(const std::shared_ptr<ast::Node>& node) {
        if (!node) {
            return;
        }
        auto one_element = [&](auto it) {
            replace_specify_endian(*it);
            auto a = ast::tool::extract_config(*it, ast::tool::ExtractMode::assign);
            if (!a) {
                return;
            }
            if (a->name != "input.endian") {
                return;
            }
            auto b = ast::cast_to<ast::Binary>(*it);
            ast::as<ast::MemberAccess>(b->left)->member->usage = ast::IdentUsage::reference_builtin_fn;
            *it = std::make_shared<ast::SpecifyEndian>(std::move(b), std::move(a->arguments[0]));
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
            one_element(&s->statement);
            return;
        }
        ast::traverse(node, [&](auto&& f) {
            replace_specify_endian(f);
        });
    }
}  // namespace brgen::middle
