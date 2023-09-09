/*license*/
#pragma once
#include "core/ast/ast.h"
#include "core/writer/section.h"
#include "core/common/error.h"
#include "core/common/expected.h"
#include "core/writer/bit_io.h"
#include "core/writer/context.h"

namespace brgen::go_lang {
    using Context = writer::Context;

    result<writer::SectionPtr> entry(Context& w, std::shared_ptr<ast::Program>& p);
}  // namespace brgen::go_lang
