/*license*/
#pragma once
#include "core/ast/ast.h"
#include "core/writer/section.h"

namespace brgen::cpp_lang {

    struct Context {
        writer::SectionPtr w;
    };

    void entry(Context& w, std::shared_ptr<ast::Program>& p);
}  // namespace brgen::cpp_lang
