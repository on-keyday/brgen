/*license*/
#pragma once
#include "nodes.h"

#include <vector>

// 親から子へ辿る。元の AST の traverse.h に当たるもの。
//
//   traverse(a, fmt, [&](auto child) { ... });          // 子を 1 段だけ
//   visit_all(a, fmt, [&](auto n) { ... });             // 部分木を先行順で全部
//   visit_all(a, fmt, [&](auto n) { return descend; }); // false を返すとそこで打ち切る
//
// weak は所有辺ではないので渡さない。辿ると belong や base で循環する。
// 空 (null) のフィールドも渡さない。fn が受け取るのは Node だけで、どの
// フィールドが空だったかは分からないので、渡しても飛ばす以外にできることが無い。
// 埋まっていないフィールドを見たいなら名前が要るので、NodeData::for_each_field を
// 直に使う (printer.h の show_null がそうしている)。
// 名前でピンポイントに取るのは access.h の field<"..."> 側。あちらはパスが
// コンパイル時に決まるので、深さが実行時に決まる走査はこちらでやる。

namespace brgen::nast {

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

    // 子を 1 段。fn は Node<X> を受ける (X は schema に書かれた型)。
    template <class T, class F>
    constexpr void traverse(Arena& a, Node<T> id, F&& fn) {
        auto* h = a.header_at(id.id());
        if (!h) {
            return;
        }
        auto index = h->data_index;
        visit_node_type(h->type, [&](auto tag) {
            using U = typename decltype(tag)::type;
            if (auto* d = a.template data_at<U>(index)) {
                d->for_each_field([&](const char*, auto& v, bool weak) {
                    if (weak) {
                        return;
                    }
                    using M = std::decay_t<decltype(v)>;
                    if constexpr (node_of<M>::is_node) {
                        if (v) {
                            fn(v);
                        }
                    }
                    else if constexpr (vector_of<M>::is_vector) {
                        for (auto& e : v) {
                            if (e) {
                                fn(e);
                            }
                        }
                    }
                });
            }
        });
    }

    template <class T, class F>
    constexpr void traverse_recursive(Arena& a, Node<T> id, F&& fn) {
        auto* h = a.header_at(id.id());
        if (!h) {
            return;
        }
        auto index = h->data_index;
        visit_node_type(h->type, [&](auto tag) {
            using U = typename decltype(tag)::type;
            if (auto* d = a.template data_at<U>(index)) {
                d->for_each_field([&](const char*, auto& v, bool weak) {
                    if (weak) {
                        return;
                    }
                    using M = std::decay_t<decltype(v)>;
                    if constexpr (node_of<M>::is_node) {
                        if (v) {
                            fn(fn, v);
                        }
                    }
                    else if constexpr (vector_of<M>::is_vector) {
                        for (auto& e : v) {
                            if (e) {
                                fn(fn, e);
                            }
                        }
                    }
                });
            }
        });
    }

    // 部分木を先行順で。fn が bool を返す形なら false で子を見ない。
    template <class T, class F>
    constexpr void visit_all(Arena& a, Node<T> id, F&& fn) {
        if (!id) {
            return;
        }
        if constexpr (std::is_convertible_v<decltype(fn(id)), bool>) {
            if (!fn(id)) {
                return;
            }
        }
        else {
            fn(id);
        }
        traverse(a, id, [&](auto child) {
            visit_all(a, child, fn);
        });
    }

}  // namespace brgen::nast
