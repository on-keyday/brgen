
#include "define_visitor.hpp"
#include "../../lowering/self_ref.hpp"
DEFINE_VISITOR(Reference) {
    DEFAULT_HANDLER()
    ON_CODEGEN() {
        auto name = ident_text(ctx.a, node.ref(ctx.a)->name);
        // 受け手を足す規則はここ 1 つ (lowering/self_ref)。継ぎ目は
        // MemberAccess と同じ separator で、受け手だけ `->` にはしない —
        // その先が `x.y` と続くので `self->x->y` になる。参照外しが要る
        // 言語は spelling 側に畳む (`(*this)`)。
        brgen::nast::lowering::Context lc{ctx.a, ctx.t};
        if (brgen::nast::lowering::receiver_field(lc, node)) {
            auto& spelling = ctx.config().Self.spelling;
            if (spelling.empty()) {
                HANDLE_UNHANDLED();
            }
            return CODE_AT(node, spelling, ctx.config().MemberAccess.separator, name);
        }
        return CODE_AT(node, name);
    }
    ON_UNHANDLED_DEFAULT()
}
