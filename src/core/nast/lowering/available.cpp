/*license*/
#include "available.hpp"
#include "predicate.hpp"
#include "../node/build.h"
#include "../node/compare.h"
#include "../node/util.h"

namespace brgen::nast::lowering {

    namespace {
        // その field があるか。分岐の中の分岐で宣言されていれば、型がまた
        // UnionType になっているので、内側の条件まで降りて掛け合わせる。
        Node<Expr> field_available(Context& c, Node<Field> f, Node<Type> want, lexer::Loc loc) {
            auto& a = c.a;
            Builder b{a, loc};
            if (!f) {
                return b.bool_lit(false);  // その分岐では宣言されていない
            }
            auto ty = strip_wrappers(a, f.ref(a)->type);
            if (auto u = ty.as_any<UnionType>()) {
                auto ud = u.ref(a);
                return branch_chain(c, ud->candidates, ud->cond, b.bool_type(), b.bool_lit(false),
                                    loc, [&](auto cd) {
                                        return field_available(c, cd->field, want, loc);
                                    });
            }
            if (!want) {
                return b.bool_lit(true);
            }
            // 型を訊かれている形 (`available(x, u8)`)。候補の型が一致する分岐
            // だけが真。
            return b.bool_lit(equivalent(a, ty, strip_wrappers(a, want)));
        }
    }  // namespace

    Node<Expr> lower_available(Context& c, Node<Available> av) {
        auto& a = c.a;
        if (!av) {
            return nullref;
        }
        if (auto* got = c.tables.table<LoweredAvailable>().get(av)) {
            return got->expr;
        }
        auto d = av.ref(a);
        if (!d->target) {
            return nullref;
        }
        auto loc = av.ref(a).loc();
        Builder b{a, loc};

        auto u = strip_wrappers(a, d->target.ref(a)->type).as_any<UnionType>();
        if (!u) {
            // 分岐で宣言された field ではない = いつでもある。
            return b.bool_lit(true);
        }
        // 裸の target は bind/receiver がレシーバを実体化した形で来る。base が
        // Self でなければ原文で修飾されていたもの。
        if (!referenced_name(a, d->target)) {
            return nullref;  // 修飾された target。条件を載せ替える形が未決 (available.hpp)
        }
        auto ud = u.ref(a);
        Node<Type> want = d->selected_type ? d->selected_type.ref(a)->literal : nullref;
        auto e = branch_chain(c, ud->candidates, ud->cond, b.bool_type(), b.bool_lit(false), loc,
                              [&](auto cd) { return field_available(c, cd->field, want, loc); });
        if (e) {
            c.tables.table<LoweredAvailable>().set(av, LoweredAvailable{.expr = e});
        }
        return e;
    }

}  // namespace brgen::nast::lowering
