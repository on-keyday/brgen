/*license*/
#include "self_ref.hpp"
#include "../node/build.h"
#include "../node/util.h"

namespace brgen::nast::lowering {

    Node<Expr> field_ref(Context& c, Node<Field> f) {
        auto& a = c.a;
        if (!f) {
            return nullref;
        }
        auto text = ident_text(a, f.ref(a)->name);
        if (text.empty()) {
            return nullref;  // 無名 field は名前で指せない
        }
        Builder b{a, f.ref(a).loc()};
        auto ref = b.ref(text, f.ref(a)->type);
        // 名前の解決先は分かっているので表に入れる。宣言側の Ident を使い
        // 回すと「宣言」と「使用」が同じノードになってしまう。
        c.tables.table<Resolution>().set(ref.as_any<Reference>().ref(a)->name,
                                         Resolution{.target = f});
        return ref;
    }

    Node<Field> receiver_field(Context& c, Node<Reference> ref) {
        if (!ref) {
            return nullref;
        }
        auto* res = c.tables.table<Resolution>().get(ref.ref(c.a)->name);
        if (!res) {
            return nullref;
        }
        return res->target.as_any<Field>();
    }

    Node<Expr> self_ref(Context& c, Node<Format> owner, lexer::Loc loc) {
        auto n = c.a.make<Self>(loc);
        if (owner) {
            n->type = owner.ref(c.a)->struct_type;
        }
        return n;
    }

}  // namespace brgen::nast::lowering
