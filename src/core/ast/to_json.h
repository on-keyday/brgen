/*license*/
#pragma once
#include "node/ast.h"
#include "node/translated.h"
#include "node/traverse.h"

namespace brgen::ast {
    struct SymbolMap {
        std::map<std::shared_ptr<Node>, size_t> index;
        std::vector<std::shared_ptr<Node>> node;
        Debug obj;

        void encode(std::shared_ptr<Node>& node) {
            visit(node, [](auto&& f) {

            });
        }
    };

}  // namespace brgen::ast
