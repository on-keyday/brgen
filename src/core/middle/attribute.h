/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>

namespace brgen::middle {

    struct TypeAttribute {
        void check_recursive_reference(const std::shared_ptr<ast::Node>& node) {
            auto traverse_fn = [&](auto&& f, const std::shared_ptr<ast::Node>& n) -> void {
                if (auto t = ast::as<ast::StructType>(n); t) {
                }
            };
            ast::traverse(node, traverse_fn);
        }

        void apply_is_finally_integer_union(const std::shared_ptr<ast::Node>& node) {
        }
    };
}  // namespace brgen::middle