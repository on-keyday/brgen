/*license*/
#include "type_size.hpp"
#include "../lowering/predicate.hpp"
#include "../node/build.h"
#include "../node/util.h"

#include <fnet/util/base64.h>

namespace brgen::nast::bind {

    namespace {
        constexpr TypeSize fixed(std::uint64_t bits) {
            return TypeSize{.kind = SizeKind::fixed, .bits = bits};
        }
        constexpr TypeSize dynamic(Node<Expr> e = nullref) {
            return TypeSize{.kind = SizeKind::dynamic, .bits_expr = e};
        }
        constexpr TypeSize unknown() {
            return TypeSize{.kind = SizeKind::unknown};
        }
    }  // namespace

    // その幅を式にする。fixed はリテラル、dynamic は持っている式。
    Node<Expr> SizeAnalysis::as_expr(TypeSize s, lexer::Loc loc) {
        if (s.kind == SizeKind::fixed) {
            return Builder{a, loc}.lit(s.bits);
        }
        if (s.kind == SizeKind::dynamic) {
            return s.bits_expr;  // 書けなかったものは null のまま伝わる
        }
        return nullref;
    }

    // 幅を足す。片方でも決まらなければ全体も決まらない。dynamic は
    // 「実行時に決まる」で unknown より情報があるので、混ざったら弱いほう
    // (unknown) に落とす。
    TypeSize SizeAnalysis::add(TypeSize l, TypeSize r, lexer::Loc loc) {
        if (l.kind == SizeKind::unknown || r.kind == SizeKind::unknown) {
            return unknown();
        }
        if (l.kind == SizeKind::fixed && r.kind == SizeKind::fixed) {
            return fixed(l.bits + r.bits);
        }
        Builder b{a, loc};
        return dynamic(b.bin(BinaryOp::add, as_expr(l, loc), as_expr(r, loc), b.int_type(64)));
    }

    // 分岐の幅を畳む。全部 fixed で同じ値ならその値。揃わないときは
    // 「どの分岐を通ったか」に依るので、式には書けない (条件式を合成すれば
    // 書けるが、それは分岐の条件を評価できる文脈でしか意味を持たない)。
    TypeSize SizeAnalysis::merge_branch(std::optional<TypeSize> acc, TypeSize s) {
        if (s.kind == SizeKind::unknown) {
            return unknown();
        }
        if (s.kind == SizeKind::dynamic) {
            return dynamic();
        }
        if (!acc) {
            return s;
        }
        if (acc->kind != SizeKind::fixed || acc->bits != s.bits) {
            return dynamic();
        }
        return s;
    }

    // 型だけでは書けない幅を、値の名前で呼ぶ。`bit_sizeof(items)`。
    //
    // 要素ごとに幅が違う配列や、分岐で幅が揃わない field がこれに当たる。
    // 型からは決まらないが値からは決まるので、値を指して渡す。展開 (要素を
    // 走って足す) は lowering の仕事で、EBM が "dynamic sizeof is not
    // supported yet" と断っているのがまさにその段。available(x) と同じ形 —
    // 値に対する述語で、意味は式評価器ではなく lowering 側にある。
    //
    // ビット単位のほうを使うのは、幅がバイトの整数倍とは限らないため
    // (sizeof はバイト単位なので、ビット境界を跨ぐ field を表せない)。
    Node<Expr> SizeAnalysis::size_of_value(Node<Field> f, lexer::Loc loc) {
        auto name = f.ref(a)->name;
        auto text = ident_text(a, name);
        if (text.empty()) {
            return nullref;  // 無名 field は指す名前が無い
        }
        // 参照は新しく作る。宣言側の Ident を使い回すと「宣言」と「使用」が
        // 同じノードになってしまう。解決先は分かっているので表に入れる。
        Builder b{a, loc};
        auto ref = b.ref(text, f.ref(a)->type);
        tables.table<Resolution>().set(ref.as_any<Reference>().ref(a)->name,
                                       Resolution{.target = f});
        auto sz = a.make<BitSizeof>(loc);
        sz->target = ref;
        sz->type = b.int_type(64);
        return sz;
    }

    // field 1 つぶんの幅。型だけで書けなければ値の名前で呼ぶ。
    TypeSize SizeAnalysis::field_size(Node<Field> f, lexer::Loc loc) {
        auto s = size_of(f.ref(a)->type);
        if (s.kind == SizeKind::dynamic && !s.bits_expr) {
            s.bits_expr = size_of_value(f, loc);
        }
        return s;
    }

    // 分岐で幅が揃わないとき、幅は「どの分岐を通ったか」で決まる。それは
    // 条件式そのものなので、三項で書ける:
    //
    //   cond1 ? w1 : (cond2 ? w2 : w_default)
    //
    // 条件は候補が持っているものをそのまま指す (複製すると中の名前が
    // Resolution 表に載らず解決先を失う)。既定の分岐が無ければ最後は 0 =
    // どれも通らなかった経路。
    //
    // この式が評価できるのは、条件が見ている field が既に読めている文脈に
    // 限られる。decode の途中では成り立つが、encode 時の条件式の意味論は
    // まだ決まっていない (docs/requires_direction.md の段階 2)。式として
    // 持っておくこと自体は、その決着とは独立に意味がある。
    // 分岐の条件を式にする。match の分岐は「パターン」であって比較ではない
    // (parse.cpp は `kind == 1` を作らず 1 をそのまま条件に置く) ので、比較を
    // 実体化する。範囲パターンの展開も含めて lowering/predicate に置いてある
    // — 幅の式に使うのも、文に落とすのも、要る述語は同じもの。
    Node<Expr> SizeAnalysis::branch_cond(Node<Expr> subject, Node<Expr> pattern, lexer::Loc loc) {
        lowering::Context lc{a, tables};
        return lowering::branch_predicate(lc, subject, pattern);
    }

    template <class Cands>
    Node<Expr> SizeAnalysis::branch_expr(const Cands& candidates, Node<Expr> subject, lexer::Loc loc,
                                         auto&& width_of) {
        Node<Expr> els = Builder{a, loc}.lit(0);
        bool have_default = false;
        // 後ろから積む。既定の分岐 (cond なし) は else の位置に入る。
        std::vector<std::pair<Node<Expr>, Node<Expr>>> arms;
        for (auto& c : candidates) {
            auto cd = c.ref(a);
            auto w = width_of(cd);
            if (!w) {
                return nullref;  // 1 つでも書けなければ全体も書けない
            }
            if (is_default_cond(a, cd->cond)) {
                els = w;
                have_default = true;
                continue;
            }
            arms.push_back({branch_cond(subject, cd->cond, loc), w});
        }
        (void)have_default;
        Builder b{a, loc};
        for (auto it = arms.rbegin(); it != arms.rend(); ++it) {
            els = b.cond(it->first, it->second, els, b.int_type(64));
        }
        return els;
    }

    // 分岐の主語。match なら比較の左辺、if なら無い (条件がそのまま述語)。
    Node<Expr> SizeAnalysis::match_subject(Node<ConditionalExpr> base) {
        if (auto m = base.as_any<Match>()) {
            return m.ref(a)->condition;
        }
        return nullref;
    }

    TypeSize SizeAnalysis::put(Node<Type> t, TypeSize s) {
        tables.table<TypeSize>().set(t, s);
        analyzed++;
        return s;
    }

    TypeSize SizeAnalysis::size_of(Node<Type> t) {
        if (!t) {
            return unknown();
        }
        if (auto* got = tables.table<TypeSize>().get(t)) {
            return *got;
        }
        if (!in_progress.insert(t.id()).second) {
            // 解いている最中に戻ってきた = 循環 (format A が A を含む)。
            // 表には置かない — 外側の解決が終われば、そちらの結果で決まる。
            return unknown();
        }
        struct Pop {
            std::unordered_set<std::uint32_t>& s;
            std::uint32_t id;
            ~Pop() {
                s.erase(id);
            }
        } pop{in_progress, t.id()};

        auto loc = a.header_at(t.id())->loc;

        // 型パラメータ。実体は instantiation ごとに変わるが、幅は
        // bit_sizeof(<T>) で書ける。これがあるので monomorphize の前でも
        // 「T が何ビットか」を運べる。
        if (auto id = t.as_any<IdentType>()) {
            if (auto* r = tables.table<Resolution>().get(id.ref(a)->ident)) {
                if (r->target.as_any<TypeParameter>()) {
                    Builder b{a, loc};
                    auto tl = a.make<TypeLiteral>(loc);
                    tl->literal = t;
                    tl->type = a.make<MetaType>(loc);
                    auto sz = a.make<BitSizeof>(loc);
                    sz->target = tl;
                    sz->type = b.int_type(64);
                    return put(t, dynamic(sz));
                }
            }
        }

        // 名前の包みは実体に降りる。書かれ方は幅に効かない。
        auto stripped = strip_wrappers(a, t);
        if (stripped != t) {
            return put(t, size_of(stripped));
        }

        if (auto i = t.as_any<IntType>()) {
            return put(t, fixed(i.ref(a)->bit_size));
        }
        if (auto f = t.as_any<FloatType>()) {
            return put(t, fixed(f.ref(a)->bit_size));
        }
        if (auto e = t.as_any<EnumType>()) {
            // 実体は下地の型。下地が書かれていない enum は幅が決まらない。
            auto base = e.ref(a)->base;
            if (!base) {
                return put(t, unknown());
            }
            return put(t, size_of(base.ref(a)->base_type));
        }
        if (auto s = t.as_any<StrLiteralType>()) {
            // magic の幅は綴りではなくバイト数。binary_value は parse.cpp が
            // base64 で入れているので、長さを取るには復号する (綴りの逃がしも
            // base64 の膨張も、どちらも数えたい値と違う)。
            auto base = s.ref(a)->base;
            if (!base) {
                return put(t, unknown());
            }
            std::string decoded;
            if (!futils::base64::decode(base.ref(a)->binary_value, decoded)) {
                return put(t, unknown());
            }
            return put(t, fixed(std::uint64_t(decoded.size()) * 8));
        }
        if (auto arr = t.as_any<ArrayType>()) {
            auto r = arr.ref(a);
            if (!r->length) {
                // 末尾まで。入力が尽きるまでなので幅の話にならない。
                return put(t, unknown());
            }
            if (r->length.as_any<Range>()) {
                // `[..]` は範囲ノードで来る。長さの式ではなく「入力が尽きる
                // まで」なので、掛け算の相手にしてはいけない。
                return put(t, unknown());
            }
            auto elem = size_of(r->element_type);
            if (elem.kind == SizeKind::unknown) {
                return put(t, unknown());
            }
            if (elem.kind != SizeKind::fixed) {
                // 要素ごとに幅が違う配列。全体は「要素の幅の和」であって
                // `要素の幅 * 個数` ではない。和を書くには要素を走る畳み込みが
                // 要るが、式の語彙に無い (書けたとしても、要素の幅の式は
                // `elem.len` のように「どの要素か」に依存するので、束縛する
                // 変数も一緒に要る)。掛け算で誤魔化さず、式なしで返す。
                return put(t, dynamic());
            }
            if (auto n = const_uint(a, tables, r->length)) {
                return put(t, fixed(elem.bits * *n));
            }
            // 要素が固定幅なので、全体は `要素の幅 * 長さ` で書ける。長さの式は
            // 元の木のノードをそのまま指す (複製すると中の名前が Resolution 表に
            // 載っていない別ノードになり、解決先を辿れなくなる)。同じノードを
            // 2 か所から指す形になるが、この式は木からは辿れず表からしか来ない。
            return put(t, dynamic(Builder{a, loc}.bin(BinaryOp::mul, Builder{a, loc}.lit(elem.bits),
                                                      r->length, Builder{a, loc}.int_type(64))));
        }
        if (auto st = t.as_any<StructType>()) {
            if (auto fmt = st.ref(a)->base.as_any<Format>()) {
                return put(t, format_size(fmt));
            }
            // state は入出力の並びに現れない。
            return put(t, unknown());
        }
        if (auto ist = t.as_any<InlineStructType>()) {
            auto fmt = ist.ref(a)->inlined_format;
            if (!fmt) {
                return put(t, unknown());
            }
            return put(t, format_size(fmt));
        }
        if (auto su = t.as_any<StructUnionType>()) {
            // 分岐。binder が候補ごとに inner_struct (その分岐の block) を
            // 持たせているので、幅は分岐の body を畳めば決まる。CFG は要らない
            // — 経路が要るのはビットの畳み込み (どこで 8 の倍数に達するか) で
            // あって、幅そのものは木の再帰で出る。
            auto r = su.ref(a);
            std::optional<TypeSize> acc;
            bool has_default = false;
            for (auto& c : r->candidates) {
                auto cd = c.ref(a);
                if (is_default_cond(a, cd->cond)) {
                    has_default = true;  // `else` / `..` があるので必ずどれかを通る
                }
                auto merged = merge_branch(acc, inner_size(cd->inner_struct));
                if (merged.kind == SizeKind::unknown) {
                    return put(t, unknown());
                }
                if (merged.kind == SizeKind::dynamic) {
                    // 揃わない (か、分岐の中が実行時)。条件で場合分けして書く。
                    return put(t, dynamic(branch_expr(r->candidates, match_subject(r->base), loc,
                                                      [&](auto cd) {
                                                          return as_expr(inner_size(cd->inner_struct), loc);
                                                      })));
                }
                acc = merged;
            }
            if (!acc) {
                return put(t, unknown());
            }
            if (!has_default && acc->bits != 0) {
                // 既定の分岐が無い = どれも通らない経路がある。そのとき幅は 0 に
                // なるので、分岐側と揃わない限り固定ではない。Match の値による
                // 網羅は見ていない (見るなら列挙の全値を覆うかの判定が要る)。
                return put(t, dynamic(branch_expr(r->candidates, match_subject(r->base), loc,
                                                  [&](auto cd) {
                                                      return as_expr(inner_size(cd->inner_struct), loc);
                                                  })));
            }
            return put(t, *acc);
        }
        if (auto u = t.as_any<UnionType>()) {
            // 同名 field の合流。実体は StructUnionType 側の分岐なので、
            // ここでは候補の型が揃うかだけを見る。
            auto r = u.ref(a);
            std::optional<TypeSize> acc;
            for (auto& c : r->candidates) {
                auto f = c.ref(a)->field;
                if (!f) {
                    // その分岐には現れない = 0 ビット。場合分けで書く。
                    return put(t, dynamic(branch_expr(r->candidates, r->cond, loc, [&](auto cd) {
                        return cd->field ? as_expr(size_of(cd->field.ref(a)->type), loc)
                                         : Builder{a, loc}.lit(0);
                    })));
                }
                auto merged = merge_branch(acc, size_of(f.ref(a)->type));
                if (merged.kind == SizeKind::unknown) {
                    return put(t, unknown());
                }
                if (merged.kind == SizeKind::dynamic) {
                    return put(t, dynamic(branch_expr(r->candidates, r->cond, loc, [&](auto cd) {
                        return cd->field ? as_expr(size_of(cd->field.ref(a)->type), loc)
                                         : Builder{a, loc}.lit(0);
                    })));
                }
                acc = merged;
            }
            return put(t, acc ? *acc : unknown());
        }
        if (t.as_any<VoidType>()) {
            return put(t, fixed(0));
        }
        // bool / range / optional / generic / stream / meta / 関数型など。
        // どれも並びに置かれる型ではないので幅を持たない。
        return put(t, unknown());
    }

    // format の幅。並ぶのは binder が集めた field で、body の文ではない
    // (分岐は 1 つの合成 field に畳まれていて、body には元の式が残っている)。
    TypeSize SizeAnalysis::format_size(Node<Format> fmt) {
        auto* state = tables.table<FormatState>().get(fmt);
        if (!state) {
            return unknown();
        }
        auto loc = a.header_at(fmt.id())->loc;
        auto total = fixed(0);
        for (auto& f : state->fields) {
            if (!is_layout_field(a, f)) {
                continue;
            }
            total = add(total, field_size(f, loc), loc);
        }
        return total;
    }

    // 分岐 1 本の幅。中で更に分岐していても、その分も binder が合成 field に
    // しているので同じ扱いで足せる。
    TypeSize SizeAnalysis::inner_size(Node<BodyStatement> block) {
        if (!block) {
            return unknown();
        }
        auto* inner = tables.table<InnerStruct>().get(block);
        if (!inner) {
            return unknown();
        }
        auto loc = a.header_at(block.id())->loc;
        auto total = fixed(0);
        for (auto& f : inner->fields) {
            if (!is_layout_field(a, f)) {
                continue;
            }
            total = add(total, field_size(f, loc), loc);
        }
        return total;
    }

    void SizeAnalysis::run() {
        // 合成した式もアリーナに積まれるので、走査の範囲は先に固定する。
        auto last = a.node_count();
        for (std::uint32_t id = 1; id <= last; id++) {
            auto* h = a.header_at(id);
            if (!h) {
                continue;
            }
            auto any = NodeAny::from_unique_id((std::uint64_t(h->type) << 32) | id);
            if (auto t = any.as_any<Type>()) {
                size_of(t);
            }
        }
    }

}  // namespace brgen::nast::bind
