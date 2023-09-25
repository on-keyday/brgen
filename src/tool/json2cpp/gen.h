/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/writer/section.h>
#include <core/ast/tool/extract_config.h>

namespace json2cpp {
    namespace ast = brgen::ast;
    struct Generator {
        brgen::writer::Writer w;
        std::shared_ptr<ast::Program> root;

        Generator(std::shared_ptr<ast::Program> root)
            : root(std::move(root)) {}

        void generate() {
            std::vector<std::shared_ptr<ast::Format>> formats;
            for (auto& l : root->elements) {
                if (l->node_type == ast::NodeType::format) {
                    formats.push_back(std::static_pointer_cast<ast::Format>(l));
                }
                else if (auto conf = ast::tool::extract_config(l)) {
                    if (conf->name == "config.cpp.namespace") {
                    }
                }
            }
        }
    };
}  // namespace json2cpp
