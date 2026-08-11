/*license*/
#pragma once
#include "nodes.h"
#include "traverse.h"

// ノードを比べる。3 段ある。
//
//   x.id() == y.id()          同じノードか。id を見るだけ (nodes.h)
//   identical(a, x, y)        木として同じか。全フィールドと loc まで見る
//   equivalent(a, x, y)       意味として同じか。位置と cosmetic なフィールドを飛ばす
//
// cosmetic は nodes.json 側の宣言で、weak と同じ扱い。今は Type::is_explicit だけが
// 立っている。u8 と明示的に書いたか推論されたかは、型としての意味を変えないため。
// lexer::Loc 型のフィールドは宣言なしで飛ばす。位置が意味に効くことはない。
//
// weak は所有辺ではないので、両方の比較とも id の一致だけを見て降りない。
// 降りると belong や base で循環する。

namespace brgen::nast {

    enum class CompareMode {
        identical,
        equivalent,
    };

    namespace compare_detail {

        template <class T>
        struct is_loc : std::false_type {};

        template <>
        struct is_loc<lexer::Loc> : std::true_type {};

        template <class T>
        constexpr bool compare(Arena& a, Node<T> l, Node<T> r, CompareMode mode);

        template <class M>
        constexpr bool compare_field(Arena& a, const M& lv, const M& rv, bool weak, CompareMode mode) {
            if constexpr (node_of<M>::is_node) {
                if (weak) {
                    return lv.id() == rv.id();
                }
                return compare(a, lv, rv, mode);
            }
            else if constexpr (vector_of<M>::is_vector) {
                if (lv.size() != rv.size()) {
                    return false;
                }
                for (std::size_t i = 0; i < lv.size(); i++) {
                    if (weak) {
                        if (lv[i].id() != rv[i].id()) {
                            return false;
                        }
                    }
                    else if (!compare(a, lv[i], rv[i], mode)) {
                        return false;
                    }
                }
                return true;
            }
            else {
                return lv == rv;
            }
        }

        template <class T>
        constexpr bool compare(Arena& a, Node<T> l, Node<T> r, CompareMode mode) {
            if (l.id() == r.id()) {
                return true;  // 同じノードなら中身を見るまでもない
            }
            if (l.is_null() || r.is_null()) {
                return false;
            }
            auto* lh = a.header_at(l.id());
            auto* rh = a.header_at(r.id());
            if (!lh || !rh || lh->type != rh->type) {
                return false;
            }
            if (mode == CompareMode::identical && !(lh->loc == rh->loc)) {
                return false;
            }
            bool eq = true;
            auto li = lh->data_index;
            auto ri = rh->data_index;
            visit_node_type(lh->type, [&](auto tag) {
                using U = typename decltype(tag)::type;
                auto* ld = a.template data_at<U>(li);
                auto* rd = a.template data_at<U>(ri);
                if (!ld || !rd) {
                    eq = (ld == rd);
                    return;
                }
                ld->for_each_field(*rd, [&](const char*, const auto& lv, const auto& rv,
                                            bool weak, bool cosmetic) {
                    if (!eq) {
                        return;
                    }
                    using M = std::decay_t<decltype(lv)>;
                    if (mode == CompareMode::equivalent && (cosmetic || is_loc<M>::value)) {
                        return;
                    }
                    eq = compare_field(a, lv, rv, weak, mode);
                });
            });
            return eq;
        }

    }  // namespace compare_detail

    // 木として同じか。位置も含めて全部見る。
    template <class T>
    constexpr bool identical(Arena& a, Node<T> l, Node<T> r) {
        return compare_detail::compare(a, l, r, CompareMode::identical);
    }

    // 意味として同じか。位置と cosmetic なフィールドは見ない。
    template <class T>
    constexpr bool equivalent(Arena& a, Node<T> l, Node<T> r) {
        return compare_detail::compare(a, l, r, CompareMode::equivalent);
    }

}  // namespace brgen::nast
