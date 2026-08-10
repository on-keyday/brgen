/*license*/
#pragma once
#include "nodes.h"

// 名前でノードを辿る。ebmgen の access.hpp に当たるもの。
//
//   auto body = get_path<"body">(arena, fmt);                 // Ref<Body>
//   auto st   = get_path<"body.struct_type">(arena, fmt);     // Ref<StructType>
//   auto f0   = get_path<"body.elements.0">(arena, fmt);      // Ref<Statement>
//   auto name = get_path<"name.identifier">(arena, fmt);      // std::string*
//
// 途中が null なら null (Ref なら空 Ref、スカラーなら nullptr) が返る。
// 存在しないフィールド名を書くと FieldOf の特殊化が無く、不完全型としてコンパイルエラーになる。
// 対応表 (FieldOf) と fixed_string は nodes.h 側に生成されている。

namespace brgen::nast {

    namespace path_detail {

        constexpr std::size_t head_len(std::string_view v) {
            auto found = v.find('.');
            return found == std::string_view::npos ? v.size() : found;
        }

        template <auto Path>
        consteval auto head() {
            constexpr auto n = head_len(Path.view());
            fixed_string<n + 1> out{};
            for (std::size_t i = 0; i < n; i++) {
                out.value[i] = Path.value[i];
            }
            return out;
        }

        template <auto Path>
        consteval auto tail() {
            constexpr auto v = Path.view();
            constexpr auto n = head_len(v);
            constexpr auto rest = v.size() > n ? v.size() - n - 1 : 0;
            fixed_string<rest + 1> out{};
            for (std::size_t i = 0; i < rest; i++) {
                out.value[i] = Path.value[n + 1 + i];
            }
            return out;
        }

        template <auto Path>
        consteval bool is_empty() {
            return Path.view().empty();
        }

        // 添字の区間。"elements.0" の "0" を配列の添字として扱う。
        constexpr bool all_digit(std::string_view v) {
            if (v.empty()) {
                return false;
            }
            for (auto c : v) {
                if (c < '0' || '9' < c) {
                    return false;
                }
            }
            return true;
        }

        template <auto Path>
        consteval std::size_t to_index() {
            std::size_t n = 0;
            for (auto c : Path.view()) {
                n = n * 10 + std::size_t(c - '0');
            }
            return n;
        }

        template <class T>
        struct node_of {
            static constexpr bool is_node = false;
        };

        template <class U>
        struct node_of<Node<U>> {
            static constexpr bool is_node = true;
            using type = U;
        };

        template <class T>
        struct vector_of {
            static constexpr bool is_vector = false;
        };

        template <class U>
        struct vector_of<std::vector<Node<U>>> {
            static constexpr bool is_vector = true;
            using type = U;
        };

        template <auto Path, class T>
        constexpr auto walk(Arena& a, NodeData<T>* d);

        // メンバ 1 つ分を解決してから、残りのパスへ進む。
        template <auto Rest, class M>
        constexpr auto step(Arena& a, M& member) {
            using node = node_of<M>;
            using vec = vector_of<M>;
            if constexpr (node::is_node) {
                using U = typename node::type;
                if constexpr (is_empty<Rest>()) {
                    return a.as_ref(member);
                }
                else {
                    return walk<Rest, U>(a, a.template get<U>(member));
                }
            }
            else if constexpr (vec::is_vector) {
                using U = typename vec::type;
                constexpr auto idx_seg = head<Rest>();
                static_assert(all_digit(idx_seg.view()),
                              "a vector field must be followed by an array index");
                constexpr auto i = to_index<idx_seg>();
                constexpr auto after = tail<Rest>();
                if (member.size() <= i) {
                    if constexpr (is_empty<after>()) {
                        return RefBase<Arena, U>{};
                    }
                    else {
                        return walk<after, U>(a, static_cast<NodeData<U>*>(nullptr));
                    }
                }
                if constexpr (is_empty<after>()) {
                    return a.as_ref(member[i]);
                }
                else {
                    return walk<after, U>(a, a.template get<U>(member[i]));
                }
            }
            else {
                static_assert(is_empty<Rest>(), "cannot descend into a scalar field");
                return &member;
            }
        }

        template <auto Path, class T>
        constexpr auto walk(Arena& a, NodeData<T>* d) {
            static_assert(!is_empty<Path>(), "empty path");
            constexpr auto h = head<Path>();
            constexpr auto rest = tail<Path>();
            using accessor = FieldOf<T, h>;
            // 未評価文脈なので d が null でも安全。結果の型だけを取る。
            using result = decltype(step<rest>(a, accessor::get(*d)));
            if (!d) {
                return result{};
            }
            return step<rest>(a, accessor::get(*d));
        }

    }  // namespace path_detail

    // 文字列リテラルから fixed_string を推論させるため、auto ではなく
    // クラステンプレートを非型引数の型に置く (auto だと const char* に減衰して通らない)。
    template <fixed_string Path, class T>
    constexpr auto get_path(Arena& a, Node<T> id) {
        return path_detail::walk<Path, T>(a, a.template get<T>(id));
    }

    template <fixed_string Path, class T>
    constexpr auto get_path(Arena& a, const RefBase<Arena, T>& ref) {
        return get_path<Path, T>(a, ref.id());
    }

}  // namespace brgen::nast
