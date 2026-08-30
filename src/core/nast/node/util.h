/*license*/
#pragma once
#include "nodes.h"

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
