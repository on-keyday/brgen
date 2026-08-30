
#include "define_visitor.hpp"
DEFINE_VISITOR(IntLiteral) {
    Ref<IntLiteral> r = node.ref(ctx.a);
    return CODE_AT(node, r->value);
}