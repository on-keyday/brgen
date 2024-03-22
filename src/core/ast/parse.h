/*license*/
#pragma once
#include "ast.h"
#include "stream.h"

namespace brgen::ast {
    struct ParseOption {
        bool collect_comments = false;
        bool error_tolerant = false;
    };
    std::shared_ptr<ast::Program> parse(Stream& stream, LocationError* err_or_warn, ParseOption option = {});
}  // namespace brgen::ast
