/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <algorithm>
#include <set>

namespace brgen::middle {
    void resolve_state_dependency(const std::shared_ptr<ast::Node>& node) {
        std::set<ast::Format*> fmts;
        auto traverse_fn = [&](auto&& f, const std::shared_ptr<ast::Node>& n) -> void {
            if (auto fmt = ast::as<ast::Format>(n)) {
                if (fmts.find(fmt) != fmts.end()) {
                    return;  // already visited
                }
                fmts.insert(fmt);
                for (auto& d : fmt->depends) {
                    auto p = d.lock();
                    if (p) {
                        auto l = p->ident->base.lock();
                        if (auto b = ast::as<ast::Format>(l)) {
                            f(f, l);
                            fmt->state_variables.insert(fmt->state_variables.end(), b->state_variables.begin(), b->state_variables.end());
                        }
                    }
                    auto kept = std::unique(fmt->state_variables.begin(), fmt->state_variables.end(), [](auto&& a, auto&& b) {
                        return a.lock() == b.lock();
                    });
                    fmt->state_variables.erase(kept, fmt->state_variables.end());
                }
            }
            ast::traverse(n, [&](auto&& n) {
                f(f, n);
            });
        };
        traverse_fn(traverse_fn, node);
    }

}  // namespace brgen::middle
