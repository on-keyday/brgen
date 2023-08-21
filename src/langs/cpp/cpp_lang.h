/*license*/
#pragma once
#include "core/ast/ast.h"
#include "core/writer/section.h"
#include "core/common/error.h"
#include "core/common/expected.h"

namespace brgen::cpp_lang {

    struct Context {
        bool last_should_be_return = false;

        auto set_last_should_be_return(bool b) {
            bool old = last_should_be_return;
            last_should_be_return = b;
            return utils::helper::defer([=, this] {
                last_should_be_return = old;
            });
        }
    };

    result<writer::SectionPtr> entry(Context& w, std::shared_ptr<ast::Program>& p);
}  // namespace brgen::cpp_lang
