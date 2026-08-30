
#include "define_visitor.hpp"
DEFINE_VISITOR(IntLiteral) {
    DEFAULT_HANDLER()
    ON_CODEGEN() {
        Ref<IntLiteral> r = node.ref(ctx.a);
        return CODE_AT(node, r->value);
    }
    ON_UNHANDLED_DEFAULT()
}