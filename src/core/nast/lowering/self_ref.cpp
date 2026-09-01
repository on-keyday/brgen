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
        auto loc = f.ref(a).loc();
        // 名前の解決先は分かっているので表に入れる。宣言側の Ident を使い
        // 回すと「宣言」と「使用」が同じノードになってしまう。
        auto id = a.make<Ident>(loc);
        id->identifier = std::string(text);
        c.tables.table<Resolution>().set(id, Resolution{.target = f});

        // 関数の中で宣言された field はローカルで、レシーバは付かない。
        // 判定は bind/receiver と同じ (持ち主が struct を持つ宣言かどうか)。
        auto owner = f.ref(a)->belong.as_any<NamedStructTypedStatement>();
        if (!owner) {
            auto n = a.make<Reference>(loc);
            n->name = id;
            n->type = f.ref(a)->type;
            return n;
        }
        auto self = a.make<Self>(loc);
        self->owner = owner;
        self->type = owner.ref(a)->struct_type;
        auto ma = a.make<MemberAccess>(loc);
        ma->base = self;
        ma->member = id;
        ma->type = f.ref(a)->type;
        return ma;
    }

    Node<Expr> self_ref(Context& c, Node<Format> owner, lexer::Loc loc) {
        auto n = c.a.make<Self>(loc);
        if (owner) {
            n->owner = owner;
            n->type = owner.ref(c.a)->struct_type;
        }
        return n;
    }

}  // namespace brgen::nast::lowering
