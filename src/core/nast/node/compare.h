/*license*/
#pragma once
#include "nodes.h"
#include "traverse.h"

// ノードを比べる。4 段ある。
//
//   x.id() == y.id()          同じノードか。id を見るだけ (nodes.h)
//   identical(a, x, y)        木として同じか。全フィールドと loc まで見る
//   equivalent(a, x, y)       意味として同じか。位置と cosmetic なフィールドを飛ばす
//   structural(a, x, y)       所有木として同じ形か。equivalent からさらに weak を
//                             丸ごと飛ばす。別々に parse した 2 つの木を突き合わせる
//                             ためのもの (weak の id は parse のたびに変わる)
//
// cosmetic は nodes.json 側の宣言で、weak と同じ扱い。今は Type::is_explicit だけが
// 立っている。u8 と明示的に書いたか推論されたかは、型としての意味を変えないため。
// lexer::Loc 型のフィールドは宣言なしで飛ばす。位置が意味に効くことはない。
//
// weak は所有辺ではないので、identical / equivalent は id の一致だけを見て降りない。
// 降りると belong や base で循環する。structural は一致も見ない (再導出できる辺
// なので形の同一性に効かせない。StrLiteralType の base のような「weak だが実体」
// の中身は比較から外れることに注意)。

namespace brgen::nast {

    enum class CompareMode {
        identical,
        equivalent,
        structural,
    };

    // 実体は compare.cpp に 1 つだけ置く。ここでテンプレートにすると、
    // 使う翻訳単位ごとに全ノード種ぶんの比較が実体化される。
    // typer.cpp で測ると、それだけでコンパイルが 2.0 秒から 6.5 秒になった。
    bool compare_any(Arena& a, NodeAny l, NodeAny r, CompareMode mode);

    // 木として同じか。位置も含めて全部見る。
    template <class T>
    bool identical(Arena& a, Node<T> l, Node<T> r) {
        return compare_any(a, l, r, CompareMode::identical);
    }

    // 意味として同じか。位置と cosmetic なフィールドは見ない。
    template <class T>
    bool equivalent(Arena& a, Node<T> l, Node<T> r) {
        return compare_any(a, l, r, CompareMode::equivalent);
    }

    // 所有木として同じ形か。weak も見ない。別々の parse どうしの突き合わせ用。
    template <class T>
    bool structural(Arena& a, Node<T> l, Node<T> r) {
        return compare_any(a, l, r, CompareMode::structural);
    }

}  // namespace brgen::nast
