/*license*/
#pragma once
#include <vector>
#include <string>
#include "core/ast/line_map.h"

namespace ebmcodegen {
    struct Output {
        std::vector<std::string> struct_names;
        std::vector<brgen::ast::LineMap> line_maps;
        int exit_code = 0;
    };
}  // namespace ebmcodegen
