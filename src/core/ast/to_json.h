/*license*/
#pragma once
#include "ast.h"
#include "node/traverse.h"
#include <map>

namespace brgen::ast {
    struct SymbolMap {
        std::map<std::shared_ptr<Node>, size_t> index;
        std::vector<std::shared_ptr<Node>> node;
        Debug obj;

        void encode(const std::shared_ptr<Node>& node) {
            visit(node, [](auto&& f) {
                f->dump([](std::string_view key, auto& value) {

                });
            });
        }
    };

}  // namespace brgen::ast
