/*license*/
#pragma once

#include <core/ast/ast.h>
#include <optional>
#include <vector>
#include <wrap/cout.h>
#include <core/common/file.h>

extern futils::wrap::UtfOut& cerr;

using Continuation = void (*)(std::shared_ptr<brgen::ast::Program>& prog, brgen::File* input, brgen::FileSet& fs);

// Function declarations with the AST_TEST_COMPONENT_API macro
void set_test_handler(Continuation cont);
void add_result(brgen::JSONWriter&& d);
void save_result(const char* file);
