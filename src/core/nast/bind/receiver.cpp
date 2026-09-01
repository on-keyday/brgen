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
                        // weak は所有辺ではない。指しているノードは所有側の
                        // 経路で差し替わるので、こちらから触ると二重になる。
                        if (weak) {
                            return;
                        }
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
        std::vector<std::pair<Node<Reference>, Node<NamedStructTypedStatement>>> targets;
        visit_all(a, mod, [&](NodeAny n) {
            if (auto ref = n.as_any<Reference>()) {
                if (auto owner = owner_of(a, tables, ref)) {
                    targets.push_back({ref, owner});
                }
            }
            return true;
        });

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
        materialized += repl.size();
    }

}  // namespace brgen::nast::bind
