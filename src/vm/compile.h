/*license*/
#pragma once
#include <core/ast/ast.h>
#include "vm.h"

namespace brgen::vm {
    Code compile(const std::shared_ptr<ast::Program>& prog);
    void print_code(futils::helper::IPushBacker<> out, const Code& code);
}  // namespace brgen::vm
