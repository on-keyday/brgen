/*license*/
#include "type_size.hpp"
#include "../node/util.h"

#include <fnet/util/base64.h>

namespace brgen::nast::bind {

    namespace {
        constexpr TypeSize fixed(std::uint64_t bits) {
            return TypeSize{.kind = SizeKind::fixed, .bits = bits};
        }
        constexpr TypeSize dynamic() {
            return TypeSize{.kind = SizeKind::dynamic};
        }
        constexpr TypeSize unknown() {
            return TypeSize{.kind = SizeKind::unknown};
        }

        // 幅を足す。片方でも決まらなければ全体も決まらない。dynamic は
        // 「実行時に決まる」で unknown より情報があるので、混ざったら弱い
        // ほう (unknown) に落とす。
        TypeSize add(TypeSize l, TypeSize r) {
            if (l.kind == SizeKind::unknown || r.kind == SizeKind::unknown) {
                return unknown();
            }
            if (l.kind == SizeKind::dynamic || r.kind == SizeKind::dynamic) {
                return dynamic();
            }
            return fixed(l.bits + r.bits);
        }

        // 分岐の幅を畳む。全部 fixed で同じ値ならその値、それ以外は実行時。
        TypeSize merge_branch(std::optional<TypeSize> acc, TypeSize s) {
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
    }  // namespace

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
            auto elem = size_of(r->element_type);
            if (auto* v = tables.table<ConstantValue>().get(r->length);
                v && v->kind == EvalKind::integer && !v->is_negative) {
                if (elem.kind != SizeKind::fixed) {
                    return put(t, elem.kind == SizeKind::unknown ? unknown() : dynamic());
                }
                return put(t, fixed(elem.bits * v->integer));
            }
            return put(t, dynamic());
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
                auto s = inner_size(cd->inner_struct);
                auto merged = merge_branch(acc, s);
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
        auto total = fixed(0);
        for (auto& f : state->fields) {
            total = add(total, size_of(f.ref(a)->type));
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
        auto total = fixed(0);
        for (auto& f : inner->fields) {
            total = add(total, size_of(f.ref(a)->type));
        }
        return total;
    }

    void SizeAnalysis::run() {
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
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
