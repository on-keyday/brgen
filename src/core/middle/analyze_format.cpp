/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/traverse.h>

namespace brgen::middle {
    void analyze_format(const std::shared_ptr<ast::Format>& fmt) {
        auto add_trait = [&](ast::FormatTrait t) {
            fmt->format_trait = ast::FormatTrait(size_t(fmt->format_trait) | size_t(t));
        };
        for (auto& elm : fmt->body->elements) {
            if (auto field = ast::as<ast::Field>(elm)) {
            }
        }
    }
}  // namespace brgen::middle
