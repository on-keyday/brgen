/*license*/
#pragma once
#include "core/ast/ast.h"
#include "core/writer/section.h"
#include "core/common/error.h"
#include "core/common/expected.h"

namespace brgen::cpp_lang {

    struct Context {
        writer::SectionPtr w;
    };

    result<void> entry(Context& w, std::shared_ptr<ast::Program>& p);
}  // namespace brgen::cpp_lang
