
#include "define_visitor.hpp"
#include "../../lowering/self_ref.hpp"
DEFINE_VISITOR(Reference) {
    DEFAULT_HANDLER()
    ON_CODEGEN() {
        auto name = ident_text(ctx.a, node.ref(ctx.a)->name);
        // 原文の field 参照には受け手が無い (`data :[len]u8` の len は裸)。
        // 生成コードでは要るので、ここで足す。受け手を使うほうが普通なので
        // フックではなく knob で吸収する — 綴りが違うだけの言語は
        // Self.spelling と MemberAccess.separator を設定すれば済む。
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
