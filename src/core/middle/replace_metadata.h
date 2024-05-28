/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>
#include "replacer.h"

namespace brgen::middle {

    std::optional<std::string> is_metadata(ast::Expr* data, bool indirect = false) {
        if (auto split = ast::as<ast::SpecialLiteral>(data)) {
            if (split->kind != ast::SpecialLiteralKind::config_ || !indirect) {
                return std::nullopt;
            }
            return "config";
        }
        if (auto split = ast::as<ast::MemberAccess>(data)) {
            auto ok = is_metadata(split->target.get(), true);
            if (!ok) {
                return std::nullopt;
            }
            split->member->usage = ast::IdentUsage::reference_builtin_fn;
            return *ok + "." + split->member->ident;
        }
        return std::nullopt;
    }

    // after resolve_io_operation
    inline void replace_metadata(NodeReplacer rep) {
        auto one_element = [&](auto it) {
            replace_metadata(*it);
            if (auto bin = ast::as<ast::Binary>(*it); bin && bin->op == ast::BinaryOp::assign) {
                auto meta = is_metadata(bin->left.get());
                if (!meta) {
                    return;
                }
                auto m = std::make_shared<ast::Metadata>(ast::cast_to<ast::Expr>(*it), std::move(*meta));
                m->values.push_back(bin->right);
                *it = std::move(m);
            }
            else if (auto call = ast::as<ast::Call>(*it)) {
                auto meta = is_metadata(call->callee.get());
                if (!meta) {
                    return;
                }
                auto m = std::make_shared<ast::Metadata>(ast::cast_to<ast::Expr>(*it), std::move(*meta));
                m->values = call->arguments;
                *it = std::move(m);
            }
        };
        auto each_element = [&](ast::node_list& list, std::vector<std::weak_ptr<ast::Metadata>>& metadata) {
            for (auto it = list.begin(); it != list.end(); it++) {
                one_element(it);
                if (auto m = ast::as<ast::Metadata>(*it)) {
                    metadata.push_back(ast::cast_to<ast::Metadata>(*it));
                }
            }
        };
        auto node = rep.to_node();
        if (auto a = ast::as<ast::Program>(node)) {
            each_element(a->elements, a->metadata);
            return;
        }
        if (auto b = ast::as<ast::IndentBlock>(node)) {
            each_element(b->elements, b->metadata);
            return;
        }
        if (auto s = ast::as<ast::ScopedStatement>(node)) {
            one_element(&s->statement);
            return;
        }
        ast::traverse(node, [&](NodeReplacer f) -> void {
            replace_metadata(f);
        });
    }
}  // namespace brgen::middle
