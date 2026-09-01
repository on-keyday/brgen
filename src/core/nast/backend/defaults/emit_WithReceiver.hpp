#include "define_visitor.hpp"
DEFINE_VISITOR(WithReceiver) {
    ON_CODEGEN() {
        auto d = node.ref(ctx.a);
        MAYBE(recv, ctx.visit(d->receiver));
        ctx.receivers.push_back(std::move(recv));
        auto inner = ctx.visit(d->expr);
        ctx.receivers.pop_back();
        return inner;
    }
    ON_UNHANDLED_DEFAULT()
}
