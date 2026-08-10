/*license*/
#pragma once
#include "nodes.h"

#include <optional>

// 名前でノードを辿る。ebmgen の access.hpp に当たるもの。
//
//   auto body = fmt.field<"body">();                     // Ref<Body>   (Ref は arena を持つ)
//   auto st   = fmt.field<"body.struct_type">();         // Ref<StructType>
//   auto f0   = fmt.field<"body.elements.0">();          // Ref<Statement>
//   auto all  = fmt.field<"body.elements">();            // std::vector<Node<Statement>>*
//   auto name = fmt.field<"name.identifier">();          // std::string*
//   auto st2  = node.field<"body.struct_type">(arena);   // Node は arena を渡す
//   auto id   = fmt.field<"name.identifier.optional">();  // std::optional<std::string>
//
// 終端の .optional は結果を std::optional にする。途中が null なら nullopt。
// ポインタや空 Ref を毎回検査せずに if (auto x = ...; x) と書けるようにするためのもの。
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

        // 終端に付けて、結果を std::optional にする擬似区間。
        // 値をそのまま比べたり if (auto x = ...; x) と書きたいときに使う。
        // 途中のどこかが null なら nullopt になる (walk の result{} がそれ)。
        // 同名のフィールドがあると隠れるが、nodes.json に optional という名前は無い。
        template <auto Path>
        consteval bool is_optional_marker() {
            return Path.view() == "optional";
        }

        template <class U>
        constexpr std::optional<U> as_optional(const U& v) {
            if constexpr (requires { bool(v); }) {
                if (!bool(v)) {
                    return std::nullopt;
                }
            }
            return v;
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
                else if constexpr (is_optional_marker<Rest>()) {
                    return as_optional(a.as_ref(member));
                }
                else {
                    return walk<Rest, U>(a, a.template get<U>(member));
                }
            }
            else if constexpr (vec::is_vector && is_empty<Rest>()) {
                // 配列そのもの。スカラーと同じくポインタで返す (回すのに要る)。
                return &member;
            }
            else if constexpr (vec::is_vector && is_optional_marker<Rest>()) {
                // 値で返すと配列を複製することになる。null かどうかは
                // ポインタ形で判定できるので、そちらを使わせる。
                static_assert(!is_optional_marker<Rest>(),
                              "a vector cannot take .optional; drop it and check the pointer");
            }
            else if constexpr (vec::is_vector) {
                using U = typename vec::type;
                constexpr auto idx_seg = head<Rest>();
                static_assert(all_digit(idx_seg.view()),
                              "a vector field must be followed by an array index, or end the path here");
                constexpr auto i = to_index<idx_seg>();
                constexpr auto after = tail<Rest>();
                if (member.size() <= i) {
                    if constexpr (is_empty<after>()) {
                        return RefBase<Arena, U>{};
                    }
                    else if constexpr (is_optional_marker<after>()) {
                        return std::optional<RefBase<Arena, U>>{};
                    }
                    else {
                        return walk<after, U>(a, static_cast<NodeData<U>*>(nullptr));
                    }
                }
                if constexpr (is_empty<after>()) {
                    return a.as_ref(member[i]);
                }
                else if constexpr (is_optional_marker<after>()) {
                    return as_optional(a.as_ref(member[i]));
                }
                else {
                    return walk<after, U>(a, a.template get<U>(member[i]));
                }
            }
            else {
                // スカラーはここまで来た時点で必ず在る。手前が null なら
                // walk が result{} (= nullopt) を返しているので、ここは engaged で良い。
                if constexpr (is_optional_marker<Rest>()) {
                    return std::optional<M>(member);
                }
                else {
                    // else に入れないと .optional の枝でもこれが評価される。
                    static_assert(is_empty<Rest>(), "cannot descend into a scalar field");
                    return &member;
                }
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

    // nodes.h の Node::field / Ref::field がこの名前で呼ぶ。宣言はそちらにある。
    // A をテンプレートにしてあるのは、Node の定義時点で Arena がまだ無いため。
    // 呼ぶときは field<"..."> を使う。ここを直接呼ぶ必要はない。
    template <fixed_string Path, class A, class T>
    constexpr auto node_field(A& a, Node<T> id) {
        return path_detail::walk<Path, T>(a, a.template get<T>(id));
    }

}  // namespace brgen::nast
