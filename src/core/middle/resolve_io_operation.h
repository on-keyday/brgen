/*license*/
#pragma once
#include <core/ast/traverse.h>
#include <core/common/error.h>
#include "replacer.h"
#include <core/ast/tool/extract_config.h>

namespace brgen::middle {

    result<void> resolve_io_operation(auto& node) {
        auto f = [](auto&& f, NodeReplacer node) -> void {
            auto n = node.to_node();
            if (ast::as<ast::FieldArgument>(n)) {
                return;  // special case
            }
            if (ast::as<ast::IOOperation>(n)) {
                return;  // already resolved
            }
            // traverse child first
            ast::traverse(n, [&](NodeReplacer r) {
                f(f, r);
            });
            auto name = ast::tool::extract_name(n);
            ast::IOMethod method = ast::IOMethod::unspec;
            if (name.size()) {
                if (name == "input.offset") {
                    method = ast::IOMethod::input_offset;
                }
                else if (name == "input.remain") {
                    method = ast::IOMethod::input_remain;
                }
                else if (name == "config.endian.big") {
                    method = ast::IOMethod::config_endian_big;
                }
                else if (name == "config.endian.little") {
                    method = ast::IOMethod::config_endian_little;
                }
                else if (name == "config.endian.native") {
                    method = ast::IOMethod::config_endian_native;
                }
                else {
                    return;
                }
                auto ac = ast::as<ast::MemberAccess>(n);
                ac->member->usage = ast::IdentUsage::reference_builtin_fn;
                if (method == ast::IOMethod::config_endian_big || method == ast::IOMethod::config_endian_little || method == ast::IOMethod::config_endian_native) {
                    ast::as<ast::MemberAccess>(ac->target)->member->usage = ast::IdentUsage::reference_builtin_fn;
                }
                auto a = std::make_shared<ast::IOOperation>(ast::cast_to<ast::Expr>(std::move(n)), method);
                node.replace(std::move(a));
                return;
            }
            auto conf = ast::tool::extract_config(n, ast::tool::ExtractMode::call);
            if (!conf) {
                return;
            }
            if (conf->name == "input.get") {
                method = ast::IOMethod::input_get;
            }
            else if (conf->name == "input.peek") {
                method = ast::IOMethod::input_peek;
            }
            else if (conf->name == "input.backward") {
                method = ast::IOMethod::input_backward;
            }
            else if (conf->name == "output.put") {
                method = ast::IOMethod::output_put;
            }
            else if (conf->name == "input.subrange") {
                method = ast::IOMethod::input_subrange;
            }
            else {
                return;
            }
            ast::as<ast::MemberAccess>(ast::as<ast::Call>(n)->callee)->member->usage = ast::IdentUsage::reference_builtin_fn;
            auto a = std::make_shared<ast::IOOperation>(ast::cast_to<ast::Expr>(std::move(n)), method);
            node.replace(std::move(a));
        };
        try {
            f(f, node);
        } catch (LocationError& e) {
            return unexpect(e);
        }
        return {};
    }
}  // namespace brgen::middle
