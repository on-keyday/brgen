
#include "define_visitor.hpp"
DEFINE_VISITOR(MemberAccess) {
    DEFAULT_HANDLER()
    ON_CODEGEN() {
        auto d = node.ref(ctx.a);
        MAYBE(base, ctx.visit(d->base));
        return CODE_AT(node, base, ctx.config().MemberAccess.separator,
                       ident_text(ctx.a, d->member));
    }
    ON_UNHANDLED_DEFAULT()
}
