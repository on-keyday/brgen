/*license*/
#pragma once

#include <future>
#include <core/ast/ast.h>
#include <optional>
#include <vector>
#include <wrap/cout.h>
#include <core/common/file.h>

using AstList = std::vector<std::future<std::shared_ptr<brgen::ast::Program>>>;

extern utils::wrap::UtfOut& cerr;

using Continuation = void (*)(std::shared_ptr<brgen::ast::Program>& prog, brgen::Input& input, std::string_view file_name);

// Function declarations with the AST_TEST_COMPONENT_API macro
void set_handler(Continuation cont);
void save_result(brgen::Debug&, const char* file);
