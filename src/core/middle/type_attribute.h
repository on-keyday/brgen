/*license*/
#pragma once
#include <core/ast/ast.h>

namespace brgen::middle {
    void mark_recursive_reference(const std::shared_ptr<ast::Node>& node);
    void analyze_bit_size_and_alignment(const std::shared_ptr<ast::Node>& node);
    void detect_non_dynamic_type(const std::shared_ptr<ast::Node>& node);
}
