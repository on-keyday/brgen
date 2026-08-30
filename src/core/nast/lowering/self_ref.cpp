/*license*/
#include "self_ref.hpp"
#include "../node/build.h"
#include "../node/util.h"

namespace brgen::nast::lowering {

    Node<Expr> field_access(Context& c, Node<Field> f, Node<Format> owner) {
        auto& a = c.a;
        if (!f) {
            return nullref;
        }
        auto text = ident_text(a, f.ref(a)->name);
        if (text.empty()) {
            return nullref;  // 無名 field は名前で指せない
        }
        Builder b{a, f.ref(a).loc()};
        auto self = a.make<Self>(b.loc);
        // 受け手の型は持ち主の struct 型。field は持ち主を指していないので
        // (FormatState が format -> fields の向きに持つ)、呼ぶ側が渡す。
        // 型が無くても綴りは出せるので、渡されなければ空のまま。
        if (owner) {
            self->type = owner.ref(a)->struct_type;
        }
        auto member = a.make<Ident>(b.loc);
        member->identifier = std::string(text);
        // 名前の解決先は分かっているので表に入れる。宣言側の Ident を使い
        // 回すと「宣言」と「使用」が同じノードになってしまう。
        c.tables.table<Resolution>().set(member, Resolution{.target = f});

        auto ma = a.make<MemberAccess>(b.loc);
        ma->base = self;
        ma->member = member;
        ma->type = f.ref(a)->type;
        return ma;
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

}  // namespace brgen::nast::lowering
