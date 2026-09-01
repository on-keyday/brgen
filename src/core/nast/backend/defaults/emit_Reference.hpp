#include "define_visitor.hpp"
DEFINE_VISITOR(Reference) {
    DEFAULT_HANDLER()
    ON_CODEGEN() {
        // ここに来るのはレシーバを取らない名前だけ (関数のローカル、
        // state variable、format や fn の名前)。field 参照は bind/receiver が
        // `MemberAccess{Self, 名前}` に実体化しているので MemberAccess 側で
        // 綴られる。
        return CODE_AT(node, ident_text(ctx.a, node.ref(ctx.a)->name));
    }
    ON_UNHANDLED_DEFAULT()
}
