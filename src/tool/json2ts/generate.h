#pragma once
#include <core/ast/ast.h>
namespace json2ts {
    struct Flags {
        bool use_bigint = false;
        bool no_resize = false;
        bool javascript = false;
        bool enum_to_string = false;
    };
    std::string generate(const std::shared_ptr<brgen::ast::Program>& p, Flags flags);
}  // namespace json2ts
