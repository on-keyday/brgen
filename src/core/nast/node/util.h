/*license*/
#pragma once
#include "nodes.h"

#include <number/prefix.h>

#include <optional>
#include <string_view>

// 木を歩くときの小物。段をまたいで使うものだけをここに置く。
//
// 置く基準は「複数の段から使われていること」だけで、便利そうだから足す場所
// ではない。解析の結果を要るもの (型付けや解決の結果を見て判断するもの) は
// ここに来ない — それは表を持っている側 (bind/) の仕事で、ここはノードの
// 形だけで決まることに限る。
//
// rebrgen の ebmcodegen が同じ層 (stub/util.hpp) を持っており、そこでは
// 粗い API を残しつつ細かい版を足す運用になっている (ADR 0029)。
// 同じように、ここも「ちょうどの粒度」を狙わず、要るものを足していく。

namespace brgen::nast {

    // アリーナにある型 T のノードを順に渡す。木からは辿れないものも含む
    // (合成したノード、パーサが先読みで捨てた個体)。
    //
    // 走査の範囲は先に固定する。中でノードを足す使い方 (解析が式を合成する
    // など) があり、足した分まで歩くと終わらない。
    // 範囲を明示する形。走査を何本かに分けるとき、途中で足したノードを
    // 後続の走査が拾わないよう、始める前の node_count を共有する。
    template <class T>
    void each_node(Arena& a, std::uint32_t last, auto&& fn) {
        for (std::uint32_t id = 1; id <= last; id++) {
            auto* h = a.header_at(id);
            if (!h) {
                continue;
            }
            auto any = NodeAny::from_unique_id((std::uint64_t(h->type) << 32) | id);
            if (auto n = any.template as_any<T>()) {
                fn(n);
            }
        }
    }

    template <class T>
    void each_node(Arena& a, auto&& fn) {
        each_node<T>(a, a.node_count(), fn);
    }

    // 名前を持つ文の綴り。持たない文 (無名 field / 分岐が合成した Field など)
    // では空を返す。ノードが無いときも空。
    inline std::string_view name_of(Arena& a, Node<Statement> s) {
        if (auto n = s.as_any<NamedStatement>()) {
            if (auto id = n.ref(a)->name.ref(a)) {
                return id->identifier;
            }
        }
        return {};
    }

    inline std::string_view ident_text(Arena& a, Node<Ident> name) {
        if (auto d = name.ref(a)) {
            return d->identifier;
        }
        return {};
    }

    // 配列の包みを外して要素の型を出す。[N][M]T のように重なっていても
    // 最後まで剥がす。配列でなければそのまま返す。
    //
    // 「その型が指している実体は何か」を見たいときに使う。長さは codec 側の
    // 情報で、実体の判定には効かない。
    inline Node<Type> strip_arrays(Arena& a, Node<Type> t) {
        while (auto arr = t.as_any<ArrayType>()) {
            t = arr.ref(a)->element_type;
        }
        return t;
    }

    // 名前の包みを外す。`header :UDPHeader` の型は IdentType で、指している
    // struct はその base にある。ImportedType など WrapperType の系列は全部
    // 同じ形なのでまとめて剥がす。
    //
    // 型で分岐する側 (typer の as_struct、バックエンドの型対応) は、書かれ方
    // ではなく指している実体を見たいので普通こちらが要る。
    inline Node<Type> strip_wrappers(Arena& a, Node<Type> t) {
        while (auto w = t.as_any<WrapperType>()) {
            auto base = w.ref(a)->base;
            if (!base) {
                break;  // 解決に失敗した名前。剥がせないのでそのまま返す。
            }
            t = base;
        }
        return t;
    }

    // 括弧を剥がす。綴りの都合で付いているだけなので、式の中身で分岐する
    // 側は剥がしてから見る。
    inline Node<Expr> strip_paren(Arena& a, Node<Expr> e) {
        while (auto p = e.as_any<Paren>()) {
            auto inner = p.ref(a)->expr;
            if (!inner) {
                break;
            }
            e = inner;
        }
        return e;
    }

    // その式が非負の整数として決まるならその値。
    //
    // 「長さが定数か」「回数が定数か」は解析でも lowering でも同じ問い。
    //
    // 表 (ConstantValue) を見た後に整数リテラルも直に見るのは、**合成した
    // リテラルが表に載らない**ため。表を埋めるのは evaluator の段で、lowering
    // が後から作ったノードはそこを通っていない。綴りの解釈は evaluator と
    // 同じ prefix_integer に任せる (0x / 0b / 0o が付く)。
    inline std::optional<std::uint64_t> const_uint(Arena& a, SideTables& tables, Node<Expr> e) {
        if (!e) {
            return std::nullopt;
        }
        if (auto* v = tables.table<ConstantValue>().get(e);
            v && v->kind == EvalKind::integer && !v->is_negative) {
            return v->integer;
        }
        if (auto lit = e.as_any<IntLiteral>()) {
            std::uint64_t value = 0;
            if (futils::number::prefix_integer(lit.ref(a)->value, value)) {
                return value;
            }
        }
        return std::nullopt;
    }

    // 分岐の条件が「既定」を表しているか。if/elif の else は条件なしの
    // BodyStatement で来るが、match の `..` は両端が空の Range で来る
    // (parse.cpp は match の全分岐を条件付きの ConditionalStatement にする)。
    // 表し方が 2 通りあるので、判定を 1 か所にまとめる。
    inline bool is_default_cond(Arena& a, Node<Expr> cond) {
        if (!cond) {
            return true;
        }
        if (auto r = cond.as_any<Range>()) {
            auto d = r.ref(a);
            return !d->start && !d->end;
        }
        return false;
    }

    // 代入の左辺の根まで降りる。`sstate.isA` や `arr[i].x` から `sstate` /
    // `arr` を、`input.endian` から `input` を出す。メンバアクセスと添字だけを
    // 辿る (括弧は parse が代入の左辺として弾くので通らない)。
    inline Node<Expr> assign_root(Arena& a, Node<Expr> e) {
        for (;;) {
            if (auto ma = e.as_any<MemberAccess>()) {
                e = ma.ref(a)->base;
                continue;
            }
            if (auto idx = e.as_any<Index>()) {
                e = idx.ref(a)->base;
                continue;
            }
            break;
        }
        return e;
    }

    // 代入の左辺として書ける形か。根が名前 (ident / メンバアクセス / 添字) か、
    // input / output / config のいずれか。parse の検査 (check_assignment) と
    // 解析側の「どの変数への書き込みか」の判定は、同じこの規則で決まる。
    // ref が渡されていて根が参照なら、それを入れて返す。
    inline bool is_assignable(Arena& a, Node<Expr> e, Node<Reference>* ref = nullptr) {
        auto root = assign_root(a, e);
        if (auto r = root.as_any<Reference>()) {
            if (ref) {
                *ref = r;
            }
            return true;
        }
        return bool(root.as_any<SpecialLiteral>());
    }

}  // namespace brgen::nast
