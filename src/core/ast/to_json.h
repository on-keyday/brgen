/*license*/
#pragma once
#include "ast.h"
#include "translated.h"
#include "traverse.h"

namespace brgen::ast {
    struct SymbolMap {
        std::map<std::shared_ptr<Node>, size_t> index;
        std::vector<std::shared_ptr<Node>> node;

        void collect(std::shared_ptr<Node>& node) {
        }
    };

}  // namespace brgen::ast
