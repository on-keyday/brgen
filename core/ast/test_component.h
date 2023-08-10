/*license*/
#pragma once

#include <future>
#include "ast.h"
#include <optional>
#include <vector>
#include <wrap/cout.h>

using AstList = std::vector<std::future<std::optional<std::shared_ptr<brgen::ast::Program>>>>;

#ifdef _WIN32
#ifdef AST_TEST_COMPONENT_EXPORTS
// We are building this library as a DLL.
#define AST_TEST_COMPONENT_API __declspec(dllexport)
#else
// We are using this library as a DLL.
#define AST_TEST_COMPONENT_API __declspec(dllimport)
#endif
#else
// On non-Windows platforms, no special decoration is needed.
#define AST_TEST_COMPONENT_API
#endif

AST_TEST_COMPONENT_API extern utils::wrap::UtfOut& cerr;

// Function declarations with the AST_TEST_COMPONENT_API macro
AST_TEST_COMPONENT_API AstList test_ast(bool debug = true);
AST_TEST_COMPONENT_API void save_result(brgen::Debug&, const char* file);
