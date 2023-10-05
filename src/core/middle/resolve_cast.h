/*license*/
#pragma once
#include <core/ast/traverse.h>

namespace brgen::middle {

    void resolve_cast(const std::shared_ptr<ast::Node>& node) {
        traverse(node, [](auto&& node) {
            if (auto cast = std::dynamic_pointer_cast<ast::Cast>(node)) {
                if (cast->expr->expr_type->node_type_tag == ast::NodeType::type) {
                    cast->expr->expr_type = cast->type;
                }
            }
        });
    }
}  // namespace brgen::middle
