/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <set>
#include <algorithm>
#include "metadata.h"

namespace brgen::ast::tool {
    struct FormatSorter {
       private:
        std::set<std::shared_ptr<ast::Format>> format;
        std::vector<std::shared_ptr<ast::Format>> ordered_format;
        std::map<std::shared_ptr<ast::Format>, std::set<std::shared_ptr<ast::Format>>> format_deps;

        void collect_format(const std::shared_ptr<ast::Node>& node) {
            ast::traverse(node, [&](const std::shared_ptr<ast::Node>& node) {
                if (ast::as<ast::Format>(node)) {
                    auto res = format.insert(ast::cast_to<ast::Format>(node));
                    if (res.second) {  // newly inserted
                        ordered_format.push_back(ast::cast_to<ast::Format>(node));
                        return;
                    }
                }
                collect_format(node);
            });
        }

        void collect_dependency() {
            format_deps.clear();
            for (auto& f : format) {
                auto& dep = format_deps[f] = {};
                for (auto& d : f->depends) {
                    auto l = d.lock();
                    if (!l) {
                        continue;
                    }
                    auto b = l->base.lock();
                    if (auto s = ast::as<ast::StructType>(b)) {
                        auto maybe_fmt = s->base.lock();
                        if (ast::as<ast::Format>(maybe_fmt)) {
                            dep.insert(ast::cast_to<ast::Format>(maybe_fmt));
                        }
                    }
                }
            }
        }

        void adjust_by_metadata(std::vector<std::shared_ptr<ast::Format>>& sorted) {
            std::set<std::shared_ptr<ast::Format>> handled;
            for (size_t i = 0; i < sorted.size();) {
                if (handled.find(sorted[i]) != handled.end()) {
                    i++;
                    continue;
                }
                auto m = lookup_str_metadata("config.order.after", sorted[i]->body->metadata);
                if (m && *m != sorted[i]->ident->ident) {
                    auto it = std::find_if(sorted.begin(), sorted.end(), [&](auto& f) {
                        return f->ident->ident == *m;
                    });
                    if (it != sorted.end()) {
                        // insert sorted[i] into after it
                        auto copy = sorted[i];
                        sorted.erase(sorted.begin() + i);
                        // iterator made invalid so we need to find again
                        it = std::find_if(sorted.begin(), sorted.end(), [&](auto& f) {
                            return f->ident->ident == *m;
                        });
                        sorted.insert(it + 1, copy);
                    }
                }
                handled.insert(sorted[i]);
                i++;
            }
        }

       public:
        std::vector<std::shared_ptr<ast::Format>> topological_sort(const std::shared_ptr<ast::Node>& node) {
            format.clear();
            collect_format(node);
            collect_dependency();
            std::vector<std::shared_ptr<ast::Format>> sorted;
            std::set<std::shared_ptr<ast::Format>> visited;
            auto visit = [&](auto&& visit_, const std::shared_ptr<ast::Format>& f) -> void {
                if (visited.find(f) != visited.end()) {
                    return;  // recursive so, return
                }
                visited.insert(f);
                for (auto& dep : format_deps[f]) {
                    visit_(visit_, dep);
                }
                sorted.push_back(f);
            };
            std::stable_sort(ordered_format.begin(), ordered_format.end(), [&](auto& a, auto& b) {
                return format_deps[a].size() < format_deps[b].size();
            });
            for (auto& f : ordered_format) {
                visit(visit, f);
            }
            adjust_by_metadata(sorted);
            return sorted;
        }
    };
}  // namespace brgen::ast::tool
