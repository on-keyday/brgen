/*license*/
#include "type_size.hpp"
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

    // 合成した式に付ける型。幅はビット数なので符号なし 64bit。1 つ作って
    // 使い回す (型が同じなら同じノードでよい。equivalent は形で比べる)。
    Node<Type> SizeAnalysis::bits_type(lexer::Loc loc) {
        if (!bits_type_) {
            auto t = a.make<IntType>(loc);
            t->bit_size = 64;
            t->is_signed = false;
            t->endian = Endian::unspec;
            bits_type_ = t;
        }
        return bits_type_;
    }

    Node<Expr> SizeAnalysis::lit(std::uint64_t v, lexer::Loc loc) {
        auto n = a.make<IntLiteral>(loc);
        n->value = std::to_string(v);
        n->type = bits_type(loc);
        return n;
    }

    // 二項の中身が二項なら括弧で包む。unparse は優先順位を見ずに Paren
    // ノードで括弧を出すので、包まないと `8 * (len - 8)` が
    // `8 * len - 8` と綴られて読み手が誤る (木は正しいが綴りが嘘になる)。
    Node<Expr> SizeAnalysis::wrap(Node<Expr> e, lexer::Loc loc) {
        if (!e || !e.as_any<Binary>()) {
            return e;
        }
        auto p = a.make<Paren>(loc);
        p->expr = e;
        p->type = bits_type(loc);
        return p;
    }

    Node<Expr> SizeAnalysis::bin(BinaryOp op, Node<Expr> l, Node<Expr> r, lexer::Loc loc) {
        if (!l || !r) {
            return nullref;  // 片方が書けなければ全体も書けない
        }
        auto n = a.make<Binary>(loc);
        n->op = op;
        n->left = wrap(l, loc);
        n->right = wrap(r, loc);
        n->type = bits_type(loc);
        return n;
    }

    // その幅を式にする。fixed はリテラル、dynamic は持っている式。
    Node<Expr> SizeAnalysis::as_expr(TypeSize s, lexer::Loc loc) {
        if (s.kind == SizeKind::fixed) {
            return lit(s.bits, loc);
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
        return dynamic(bin(BinaryOp::add, as_expr(l, loc), as_expr(r, loc), loc));
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

    // 型だけでは書けない幅を、値の名前で呼ぶ。`sizeof(items) * 8`。
    //
    // 要素ごとに幅が違う配列や、分岐で幅が揃わない field がこれに当たる。
    // 型からは決まらないが値からは決まるので、値を指して sizeof に渡す。
    // 展開 (要素を走って足す) は lowering の仕事で、EBM が
    // "dynamic sizeof is not supported yet" と断っているのがまさにその段。
    // available(x) と同じ形 — 値に対する述語で、意味は式評価器ではなく
    // lowering 側にある。
    //
    // sizeof はバイト単位なので 8 倍する。裏を返すと、この項はその field の
    // 幅がバイトの整数倍であることを前提にしている。
    Node<Expr> SizeAnalysis::size_of_value(Node<Field> f, lexer::Loc loc) {
        auto name = f.ref(a)->name;
        auto text = ident_text(a, name);
        if (text.empty()) {
            return nullref;  // 無名 field は指す名前が無い
        }
        // 参照は新しく作る。宣言側の Ident を使い回すと「宣言」と「使用」が
        // 同じノードになってしまう。解決先は分かっているので表に入れる。
        auto id = a.make<Ident>(loc);
        id->identifier = std::string(text);
        tables.table<Resolution>().set(id, Resolution{.target = f});
        auto ref = a.make<Reference>(loc);
        ref->name = id;
        ref->type = f.ref(a)->type;
        auto sz = a.make<Sizeof>(loc);
        sz->target = ref;
        sz->type = bits_type(loc);
        return bin(BinaryOp::mul, sz, lit(8, loc), loc);
    }

    // field 1 つぶんの幅。型だけで書けなければ値の名前で呼ぶ。
    TypeSize SizeAnalysis::field_size(Node<Field> f, lexer::Loc loc) {
        auto s = size_of(f.ref(a)->type);
        if (s.kind == SizeKind::dynamic && !s.bits_expr) {
            s.bits_expr = size_of_value(f, loc);
        }
        return s;
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
        // sizeof(<T>) で書ける。これがあるので monomorphize の前でも
        // 「T が何ビットか」を運べる (単位はバイトなので 8 倍する)。
        if (auto id = t.as_any<IdentType>()) {
            if (auto* r = tables.table<Resolution>().get(id.ref(a)->ident)) {
                if (r->target.as_any<TypeParameter>()) {
                    auto tl = a.make<TypeLiteral>(loc);
                    tl->literal = t;
                    tl->type = a.make<MetaType>(loc);
                    auto sz = a.make<Sizeof>(loc);
                    sz->target = tl;
                    sz->type = bits_type(loc);
                    return put(t, dynamic(bin(BinaryOp::mul, sz, lit(8, loc), loc)));
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
            if (auto* v = tables.table<ConstantValue>().get(r->length);
                v && v->kind == EvalKind::integer && !v->is_negative) {
                return put(t, fixed(elem.bits * v->integer));
            }
            // 要素が固定幅なので、全体は `要素の幅 * 長さ` で書ける。長さの式は
            // 元の木のノードをそのまま指す (複製すると中の名前が Resolution 表に
            // 載っていない別ノードになり、解決先を辿れなくなる)。同じノードを
            // 2 か所から指す形になるが、この式は木からは辿れず表からしか来ない。
            return put(t, dynamic(bin(BinaryOp::mul, lit(elem.bits, loc), r->length, loc)));
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
                if (!cd->cond) {
                    has_default = true;  // `else` / `..` があるので必ずどれかを通る
                }
                auto merged = merge_branch(acc, inner_size(cd->inner_struct));
                if (merged.kind != SizeKind::fixed) {
                    return put(t, merged);
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
                return put(t, dynamic());
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
                    return put(t, dynamic());  // その分岐には現れない
                }
                auto merged = merge_branch(acc, size_of(f.ref(a)->type));
                if (merged.kind != SizeKind::fixed) {
                    return put(t, merged);
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
