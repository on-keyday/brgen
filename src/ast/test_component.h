/*license*/
#pragma once

#include <future>
#include "ast.h"
#include <optional>
#include <vector>
#include <wrap/cout.h>

using AstList = std::vector<std::future<std::optional<std::shared_ptr<ast::Program>>>>;
extern utils::wrap::UtfOut& cerr;

AstList test_ast();
