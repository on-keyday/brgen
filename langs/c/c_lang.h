/*license*/
#pragma once
#include "core/ast/ast.h"
#include "core/writer/writer.h"

namespace c_lang {
    struct Context {
        writer::TreeWriter* w = nullptr;
    };
    void entry(writer::TreeWriter& w, std::shared_ptr<ast::Program>& p);
}  // namespace c_lang