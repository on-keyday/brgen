/*license*/
#pragma once
#include "../common/file.h"
#include "../common/error.h"
#include "../ast/ast.h"

namespace brgen::middle {
    result<void> analyze_type(std::shared_ptr<ast::Program>& node, LocationError* warnings);
}
