/*license*/
#include "union_layout.hpp"

#include <vector>

namespace brgen::nast::bind {

    void UnionLayoutAnalysis::run() {
        // UnionType は binder の合成でしか作られないので、アリーナ全体を
        // 走査してよい (パーサの先読みで捨てられた個体は無い)。
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (!h || h->type != NodeType::UnionType) {
                continue;
            }
            auto u = Node<UnionType>::from_unique_id((std::uint64_t(NodeType::UnionType) << 32) | id);
            if (tables.table<UnionLayout>().get(u)) {
                continue;
            }
            // common_type は fit の途中で新しいノードを作ることがあり、header の
            // ポインタが無効になるので位置は先に写しておく。
            auto loc = h->loc;

            // 候補 field の相異なる型。same_type で同一視して出現順に集める。
            // 入れ子の union (分岐の中の分岐で宣言された同名 field) は候補の
            // 型に潰さず、その候補たちの型へ降りて平らに混ぜる。ebmgen の
            // map_field (union_property.cpp) が property の derived_from を
            // 辿るのと同じ扱い。
            std::vector<Node<Type>> member_types;
            std::vector<Node<Type>> queue;
            for (auto& c : u.ref(a)->candidates) {
                auto f = c.ref(a)->field;
                if (!f) {
                    continue;  // 名前が現れない分岐の pad
                }
                queue.push_back(f.ref(a)->type);
            }
            for (std::size_t qi = 0; qi < queue.size(); qi++) {
                auto t = queue[qi];
                if (!t) {
                    continue;  // 型の無い field (error-tolerant な入力)
                }
                if (auto nested = t.as_any<UnionType>()) {
                    for (auto& nc : nested.ref(a)->candidates) {
                        if (auto nf = nc.ref(a)->field) {
                            queue.push_back(nf.ref(a)->type);
                        }
                    }
                    continue;
                }
                bool seen = false;
                for (auto& m : member_types) {
                    if (typer.same_type(m, t)) {
                        seen = true;
                        break;
                    }
                }
                if (!seen) {
                    member_types.push_back(t);
                }
            }
            if (member_types.empty()) {
                continue;
            }

            // ebmgen の clustering_properties の写し: common_type が取れる型
            // どうしに辺を張り、先行する最小の隣のクラスタへ合流させる。
            std::vector<std::size_t> cluster_of(member_types.size());
            std::vector<std::vector<std::size_t>> clusters;
            for (std::size_t i = 0; i < member_types.size(); i++) {
                std::size_t join = clusters.size();
                for (std::size_t j = 0; j < i; j++) {
                    if (typer.common_type(member_types[j], member_types[i])) {
                        join = cluster_of[j];
                        break;
                    }
                }
                if (join == clusters.size()) {
                    clusters.push_back({});
                }
                cluster_of[i] = join;
                clusters[join].push_back(i);
            }

            // クラスタの合流型。出現順に畳む。辺は推移的でないので途中で
            // 畳めなくなることが原理上はあり、そのときは表を作らず警告する。
            std::vector<Node<Type>> merged(clusters.size());
            bool ok = true;
            for (std::size_t ci = 0; ci < clusters.size(); ci++) {
                Node<Type> common;
                for (auto mi : clusters[ci]) {
                    if (!common) {
                        common = member_types[mi];
                        continue;
                    }
                    common = typer.common_type(common, member_types[mi]);
                    if (!common) {
                        err.warning(loc, "union layout: clustered types lost their common type while folding");
                        ok = false;
                        break;
                    }
                }
                merged[ci] = common;
            }
            if (!ok) {
                continue;
            }

            UnionLayout layout;
            layout.member_types = member_types;
            for (std::size_t i = 0; i < member_types.size(); i++) {
                layout.cluster_types.push_back(merged[cluster_of[i]]);
            }
            tables.table<UnionLayout>().set(u, std::move(layout));
            analyzed++;
        }
    }

}  // namespace brgen::nast::bind
