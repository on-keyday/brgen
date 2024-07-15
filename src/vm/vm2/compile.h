/*license*/
#pragma once
#include <binary/writer.h>
#include <core/ast/ast.h>

namespace brgen::vm2 {

    void compile(const std::shared_ptr<ast::Program>& node, futils::binary::writer& dst);
}