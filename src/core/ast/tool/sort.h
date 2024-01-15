/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <set>

namespace brgen::ast::tool {
    struct FormatSorter {
        std::set<std::shared_ptr<ast::Format>> format;
        std::map<std::shared_ptr<ast::Format>, std::set<std::shared_ptr<ast::Format>>> format_deps;

       private:
        void collect_format(const std::shared_ptr<ast::Node>& node) {
            ast::traverse(node, [&](const std::shared_ptr<ast::Node>& node) {
                if (ast::as<ast::Format>(node)) {
                    format.insert(ast::cast_to<ast::Format>(node));
                }
                collect_format(node);
            });
        }

       public:
        void topological_sort(const std::shared_ptr<ast::Node>& node) {
            format.clear();
            collect_format(node);
            for (auto& f : format) {
                format_deps[f] = {};
            }
        }
    };
}  // namespace brgen::ast::tool
