/*license*/
#pragma once
#include <core/common/file.h>
#include <core/ast/ast.h>

namespace brgen::test {
    // simplified version of src2json
    // this only parse success case, otherwise throw exception
    std::shared_ptr<ast::Program> src2json(FileSet& fs);
}  // namespace brgen::test
