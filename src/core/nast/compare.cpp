/*license*/
#include "compare.h"

#include "traverse.h"

namespace brgen::nast {

    namespace {

        template <class T>
        struct is_loc : std::false_type {};

        template <>
        struct is_loc<lexer::Loc> : std::true_type {};

        bool compare(Arena& a, NodeAny l, NodeAny r, CompareMode mode);

        template <class M>
        bool compare_field(Arena& a, const M& lv, const M& rv, bool weak, CompareMode mode) {
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

        bool compare(Arena& a, NodeAny l, NodeAny r, CompareMode mode) {
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

    }  // namespace

    bool compare_any(Arena& a, NodeAny l, NodeAny r, CompareMode mode) {
        return compare(a, l, r, mode);
    }

}  // namespace brgen::nast
