/*license*/
#include "enum_defined.hpp"
#include "../node/build.h"
#include "../node/util.h"

namespace brgen::nast::lowering {

    namespace {
        // `Hello.A`。解決先も入れる — 名前で引き直さずに済むようにするため
        // (lowering/self_ref の field_ref と同じ扱い)。
        Node<Expr> member_ref(Context& c, Node<Enum> en, Node<EnumMember> mem, lexer::Loc loc) {
            auto& a = c.a;
            auto enum_name = name_of(a, en);
            auto member_name = name_of(a, mem);
            if (enum_name.empty() || member_name.empty()) {
                return nullref;
            }
            Builder b{a, loc};
            auto base = b.ref(enum_name, en.ref(a)->enum_type);
            c.tables.table<Resolution>().set(base.as_any<Reference>().ref(a)->name,
                                             Resolution{.target = en});
            auto member = a.make<Ident>(loc);
            member->identifier = std::string(member_name);
            c.tables.table<Resolution>().set(member, Resolution{.target = mem});
            auto ma = a.make<MemberAccess>(loc);
            ma->base = base;
            ma->member = member;
            ma->type = en.ref(a)->enum_type;
            return ma;
        }
    }  // namespace

    Node<Expr> lower_enum_is_defined(Context& c, Node<MemberAccess> ma) {
        auto& a = c.a;
        if (!ma) {
            return nullref;
        }
        if (auto* got = c.tables.table<LoweredEnumIsDefined>().get(ma)) {
            return got->expr;
        }
        auto d = ma.ref(a);
        if (ident_text(a, d->member) != "is_defined" || !d->base) {
            return nullref;
        }
        auto et = strip_wrappers(a, d->base.ref(a)->type).as_any<EnumType>();
        if (!et) {
            return nullref;
        }
        auto en = et.ref(a)->base;
        if (!en) {
            return nullref;
        }
        auto loc = ma.ref(a).loc();
        Builder b{a, loc};
        Node<Expr> out;
        for (auto& mem : en.ref(a)->members) {
            auto rhs = member_ref(c, en, mem, loc);
            if (!rhs) {
                return nullref;
            }
            auto eq = b.bin(BinaryOp::equal, d->base, rhs, b.bool_type());
            out = out ? b.bin(BinaryOp::logical_or, out, eq, b.bool_type()) : eq;
        }
        if (!out) {
            // メンバが 1 つも無い列挙。どの値も定義済みではない。
            out = b.bool_lit(false);
        }
        c.tables.table<LoweredEnumIsDefined>().set(ma, LoweredEnumIsDefined{.expr = out});
        return out;
    }

}  // namespace brgen::nast::lowering
