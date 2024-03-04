/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>
#include <core/ast/tool/extract_config.h>
#include <core/ast/tool/extract_config.h>

namespace brgen::middle {
    inline void replace_specify_order(const std::shared_ptr<ast::Node>& node) {
        if (!node) {
            return;
        }
        auto one_element = [&](auto it) {
            replace_specify_order(*it);
            auto a = ast::tool::extract_config(*it, ast::tool::ExtractMode::assign);
            if (!a) {
                return;
            }
            if (a->name != "input.endian" && a->name != "input.bit_order" &&
                a->name != "input.bit_order.stream" &&
                a->name != "input.bit_order.mapping") {
                return;
            }
            auto b = ast::cast_to<ast::Binary>(*it);
            ast::tool::marking_builtin(b->left);
            *it = std::make_shared<ast::SpecifyOrder>(std::move(b), std::move(a->arguments[0]),
                                                      a->name == "input.endian"             ? ast::OrderType::byte
                                                      : a->name == "input.bit_order"        ? ast::OrderType::bit_both
                                                      : a->name == "input.bit_order.stream" ? ast::OrderType::bit_stream
                                                                                            : ast::OrderType::bit_mapping);
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
            replace_specify_order(f);
        });
    }
}  // namespace brgen::middle
