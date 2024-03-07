#pragma once
#include <core/ast/ast.h>
namespace json2ts {
    std::string generate(const std::shared_ptr<brgen::ast::Program>& p);
}