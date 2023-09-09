/*license*/
#pragma once
#include "expr.h"
#include "statement.h"

namespace brgen::ast {
    struct Scope {
        std::list<std::shared_ptr<Ident>> variables;
        std::list<std::shared_ptr<Field>> fields;
        std::list<std::shared_ptr<Format>> formats;
    };
}  // namespace brgen::ast
