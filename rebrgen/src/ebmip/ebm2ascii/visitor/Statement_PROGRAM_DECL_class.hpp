/*DO NOT EDIT BELOW SECTION MANUALLY*/
/*license*/
/*
  Name: Statement_PROGRAM_DECL_class
*/
/*DO NOT EDIT ABOVE SECTION MANUALLY*/

#include "../codegen.hpp"
#include "common.hpp"

DEFINE_VISITOR(Statement_PROGRAM_DECL) {
    using namespace CODEGEN_NAMESPACE;
    for (auto& b : ctx.block.container) {
        if (ctx.is(ebm::StatementKind::STRUCT_DECL, b)) {
            MAYBE_VOID(_, ctx.visit(b));
        }
    }
    return {};
}
