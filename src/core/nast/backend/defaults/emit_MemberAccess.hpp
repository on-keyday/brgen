
#include "define_visitor.hpp"
DEFINE_VISITOR(MemberAccess) {
    DEFAULT_HANDLER()
    ON_CODEGEN() {
        Ref<MemberAccess> d = node.ref(ctx.a);
        if (d->base.type() == NodeType::Self &&
            ctx.c.Self.spelling.empty() &&
            ctx.receivers.empty()) {
            return CODE_AT(node, ident_text(ctx.a, d->member));
        }
        MAYBE(base, ctx.visit(d->base));
        return CODE_AT(node, base, ctx.config().MemberAccess.separator,
                       ident_text(ctx.a, d->member));
    }
    ON_UNHANDLED_DEFAULT()
}
