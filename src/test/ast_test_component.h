/*license*/
#pragma once

#include <core/ast/ast.h>
#include <optional>
#include <vector>
#include <wrap/cout.h>
#include <core/common/file.h>

extern utils::wrap::UtfOut& cerr;

using Continuation = void (*)(std::shared_ptr<brgen::ast::Program>& prog, brgen::File* input, brgen::FileSet& fs);

// Function declarations with the AST_TEST_COMPONENT_API macro
void set_handler(Continuation cont);
void add_result(brgen::Debug&& d);
void save_result(const char* file);
