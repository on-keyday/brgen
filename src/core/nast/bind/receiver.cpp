/*license*/
#include "receiver.hpp"
#include "../node/traverse.h"

#include <unordered_map>
#include <unordered_set>

namespace brgen::nast::bind {

    namespace {
        // 参照の解決先がレシーバを取る field なら、その持ち主。
        Node<NamedStructTypedStatement> owner_of(Arena& a, SideTables& tables, Node<Reference> ref) {
            auto* res = tables.table<Resolution>().get(ref.ref(a)->name);
            if (!res) {
                return nullref;
            }
            auto f = res->target.as_any<Field>();
            if (!f) {
                return nullref;
            }
            // 関数の中で宣言された field はローカルで、レシーバは付かない。
            // `y :u8` は format の中でも関数の中でも同じ Field なので、Field で
            // あることだけでは足りず、持ち主を見る。
            auto belong = f.ref(a)->belong;
            if (!belong) {
                return nullref;
            }
            return belong.as_any<NamedStructTypedStatement>();
        }

        // 表の中から指されているノードを渡す。表そのものは書き換えない
        // (中の Node は読むだけで、差し替えはノード側のスロットで起きる)。
        void walk_tables(Arena& a, const SideTables& tables, auto&& fn) {
            // flag の表は値を持たないので、entry が来ない形でも呼ばれる。
            auto walk_entry = [&](const auto& entry) {
                entry.for_each_field([&](const char*, const auto& v, bool) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (node_of<T>::is_node) {
                        if (v) {
                            fn(NodeAny(v));
                        }
                    }
                    else if constexpr (vector_of<T>::is_vector) {
                        for (auto& e : v) {
                            if (e) {
                                fn(NodeAny(e));
                            }
                        }
                    }
                });
            };
            tables.for_each_table([&](const char*, const auto& table) {
                table.for_each_entry([&](std::uint32_t, const auto&... entry) {
                    (walk_entry(entry), ...);
                });
            });
        }

        // 差し替え表に沿ってスロットを書き換える。**ノードは作らない** —
        // arena の pool は vector で、for_each_field の間はそのノードの実体を
        // 掴んだままなので、途中で make すると移動して無効になる。作るのは
        // 走査の外で済ませてある。
        struct Rewriter {
            Arena& a;
            const std::unordered_map<std::uint32_t, Node<MemberAccess>>& repl;
            std::unordered_set<std::uint32_t> seen;

            template <class M>
            void rewrite(M& slot) {
                if constexpr (std::is_assignable_v<M&, Node<MemberAccess>>) {
                    auto it = repl.find(slot.id());
                    if (it == repl.end()) {
                        return;
                    }
                    slot = it->second;
                }
            }

            void run(NodeAny id) {
                if (!id || !seen.insert(id.id()).second) {
                    return;
                }
                visit_node_type(id.type(), [&](auto tag) {
                    using U = typename decltype(tag)::type;
                    auto d = id.as_any<U>().ref(a);
                    if (!d) {
                        return;
                    }
                    d->for_each_field([&](const char*, auto& v, bool weak) {
                        // **weak も差し替えるし、weak の先へも降りる。** 元の
                        // ノードは置き換えるので (書き換えではない)、weak を
                        // そのままにすると木から外れた参照を指し続ける。
                        // match の主語のように weak からしか指されていない式も
                        // ある。循環は seen で止まる。
                        (void)weak;
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (node_of<T>::is_node) {
                            if (v) {
                                rewrite(v);
                                run(NodeAny(v));
                            }
                        }
                        else if constexpr (vector_of<T>::is_vector) {
                            for (auto& e : v) {
                                if (e) {
                                    rewrite(e);
                                    run(NodeAny(e));
                                }
                            }
                        }
                    });
                });
            }
        };
    }  // namespace

    void MaterializeReceiver::run(Node<Module> mod) {
        // 1. 差し替える参照を集める。ここでは作らない。
        //    **weak も辿る。** match の主語のように、所有辺からは外れていて
        //    weak (UnionType.cond) からしか指されていない式がある。そこを
        //    materialize し損ねると、綴る側がレシーバ無しの参照を見る。
        std::vector<std::pair<Node<Reference>, Node<NamedStructTypedStatement>>> targets;
        std::unordered_set<std::uint32_t> seen;
        auto collect = [&](auto&& self, NodeAny id) -> void {
            if (!id || !seen.insert(id.id()).second) {
                return;
            }
            if (auto ref = id.as_any<Reference>()) {
                if (auto owner = owner_of(a, tables, ref)) {
                    targets.push_back({ref, owner});
                }
            }
            visit_node_type(id.type(), [&](auto tag) {
                using U = typename decltype(tag)::type;
                auto d = id.as_any<U>().ref(a);
                if (!d) {
                    return;
                }
                d->for_each_field([&](const char*, const auto& v, bool) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (node_of<T>::is_node) {
                        if (v) {
                            self(self, NodeAny(v));
                        }
                    }
                    else if constexpr (vector_of<T>::is_vector) {
                        for (auto& e : v) {
                            if (e) {
                                self(self, NodeAny(e));
                            }
                        }
                    }
                });
            });
        };
        collect(collect, mod);
        // 表からしか指されていないものも辿る。binder が作る union の field は
        // まだ木に繋がっておらず (FormatState/InnerStruct に入っているだけ)、
        // その型 (UnionType) が持つ match の主語をここで拾わないと、
        // 綴る側がレシーバ無しの参照を見る。
        walk_tables(a, tables, [&](NodeAny n) { collect(collect, n); });

        // 2. 置き換えるノードを作る。Ident は作り直さず持ち回す
        //    (Resolution 表のキーなので、作り直すと解決先を失う)。
        std::unordered_map<std::uint32_t, Node<MemberAccess>> repl;
        for (auto& [ref, owner] : targets) {
            auto loc = ref.ref(a).loc();
            auto self = a.make<Self>(loc);
            self->owner = owner;
            auto ma = a.make<MemberAccess>(loc);
            ma->base = self;
            ma->member = ref.ref(a)->name;
            repl.emplace(ref.id(), ma);
        }

        // 3. スロットを差し替える。元の Reference は木から外れて孤児になる。
        Rewriter rw{a, repl};
        rw.run(mod);
        walk_tables(a, tables, [&](NodeAny n) { rw.run(n); });
        materialized += repl.size();
    }

}  // namespace brgen::nast::bind
