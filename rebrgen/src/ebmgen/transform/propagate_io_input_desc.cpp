/*license*/
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include "ebmgen/common.hpp"
#include "transform.hpp"
#include "ebm/extended_binary_module.hpp"
#include "../converter.hpp"

namespace ebmgen {

    expected<void> rewrite_input_type_with_io_desc(ConverterContext& ctx, ebm::TypeRef id, bool enc, const ebm::IOInputDesc& desc);

    expected<void> propagate_io_input_desc(TransformContext& tctx, std::function<void(const char*)> timer) {
        if (!tctx.context().state().should_propagate_io_input_desc()) {
            return {};
        }
        auto& hierarchy = tctx.context().state().get_propagated_io_input_desc_hierarchy();
        if (hierarchy.empty()) {
            return {};
        }

        auto get_io_input_desc = [&](ebm::TypeRef ref) -> expected<std::optional<std::pair<ebm::TypeKind, ebm::IOInputDesc>>> {
            MAYBE(type, tctx.context().repository().get_type(ref));
            auto res = type.body.io_input_desc();
            if (res) {
                return std::make_pair(type.body.kind, *res);
            }
            return std::nullopt;
        };

        struct PropagationDesc {
            ebm::TypeKind kind;
            ebm::TypeRef type;
            ebm::IOInputDesc io_input_desc;
            const std::vector<ebm::TypeRef>* children;
            std::vector<ebm::TypeRef> parents;
            bool changed = false;
        };

        std::vector<PropagationDesc> worklist;

        std::unordered_map<ebm::TypeRef, size_t> indices;

        auto& aliases = tctx.alias_vector();
        for (auto& alias : aliases) {
            if (alias.hint == ebm::AliasHint::TYPE) {
                auto root = from_any_ref<ebm::TypeRef>(alias.from);
                MAYBE(io_input_desc, get_io_input_desc(root));
                if (io_input_desc) {
                    if (auto prop = hierarchy.find(root); prop != hierarchy.end()) {
                        size_t index = 0;
                        if (auto found = indices.find(root); found != indices.end()) {
                            index = found->second;
                            worklist[index].kind = io_input_desc->first;
                            worklist[index].io_input_desc = io_input_desc->second;
                            worklist[index].children = &prop->second;
                        }
                        else {
                            worklist.push_back({.kind = io_input_desc->first, .type = root, .io_input_desc = io_input_desc->second, .children = &prop->second});
                            index = worklist.size() - 1;
                            indices[root] = index;
                        }
                        for (auto& child : prop->second) {
                            if (auto found = indices.find(child); found == indices.end()) {
                                worklist.push_back({.type = child, .children = nullptr, .parents = {root}});
                                indices[child] = worklist.size() - 1;
                            }
                            else {
                                worklist[found->second].parents.push_back(root);
                            }
                        }
                    }
                    else {
                        worklist.push_back({.kind = io_input_desc->first, .type = root, .io_input_desc = io_input_desc->second, .children = nullptr});
                        indices[root] = worklist.size() - 1;
                    }
                }
            }
        }

        if (timer) {
            timer("collecting io input propagation worklist");
        }

        /*
        std::unordered_set<ebm::TypeRef> visited;  // parent-child pairs
        bool changed = true;

        auto propagate_desc = [&](ebm::IOInputDesc& prop1, ebm::IOInputDesc& prop2) {
            if (prop1.has_absolute_offset() || prop2.has_absolute_offset()) {
                auto old1 = prop1.has_absolute_offset();
                auto old2 = prop2.has_absolute_offset();
                prop1.has_absolute_offset(true);
                prop2.has_absolute_offset(true);
                if (!old1 || !old2) {
                    changed = true;
                }
            }
            if (prop1.has_bit_offset() || prop2.has_bit_offset()) {
                auto old1 = prop1.has_bit_offset();
                auto old2 = prop2.has_bit_offset();
                prop1.has_bit_offset(true);
                prop2.has_bit_offset(true);
                if (!old1 || !old2) {
                    changed = true;
                }
            }
            if (prop1.is_seekable() || prop2.is_seekable()) {
                auto old1 = prop1.is_seekable();
                auto old2 = prop2.is_seekable();
                prop1.is_seekable(true);
                prop2.is_seekable(true);
                if (!old1 || !old2) {
                    changed = true;
                }
            }
        };

        auto propagate = [&](auto&& propagate, PropagationDesc& work) -> expected<void> {
            auto local_propagate = [&](ebm::TypeRef target) -> expected<void> {
                auto index = indices.find(target);
                if (index == indices.end()) {
                    return unexpect_error("Target not found in indices: {}", get_id(target));
                }
                auto& parent_work = worklist[index->second];
                propagate_desc(work.io_input_desc, parent_work.io_input_desc);
                MAYBE_VOID(_, propagate(propagate, parent_work));
                return {};
            };

            if (visited.contains(work.type)) {
                return {};
            }
            visited.insert(work.type);
            auto d = futils::helper::defer([&] {
                visited.erase(work.type);
            });
            for (auto& parent : work.parents) {
                MAYBE_VOID(_, local_propagate(parent));
            }
            if (work.children) {
                for (auto& child : *work.children) {
                    MAYBE_VOID(_, local_propagate(child));
                }
            }
            return {};
        };

        for (auto& work : worklist) {
            if (work.parents.empty() == (work.children == nullptr) &&
                !tctx.context().state().get_recursive_io_input_descs().contains(work.type)) {
                // not a leaf
                continue;
            }
            if (work.parents.empty() && work.children == nullptr) {
                // leaf node, no need to propagate
                continue;
            }
            MAYBE_VOID(_, propagate(propagate, work));
        }
        */

        // --- 2. 状態伝播のヘルパー関数 (単方向の更新と変化検知) ---
        // 双方向更新を分離し、「targetがsourceから新しい状態を受け取って変化したか」を返す
        auto apply_propagation = [](ebm::IOInputDesc& target, const ebm::IOInputDesc& source) -> bool {
            bool changed = false;
            if (!target.has_absolute_offset() && source.has_absolute_offset()) {
                target.has_absolute_offset(true);
                changed = true;
            }
            if (!target.has_bit_offset() && source.has_bit_offset()) {
                target.has_bit_offset(true);
                changed = true;
            }
            if (!target.is_seekable() && source.is_seekable()) {
                target.is_seekable(true);
                changed = true;
            }
            return changed;
        };

        // --- 3. 単調ワークリスト・アルゴリズム (Queue-based Monotonic Relaxation) ---
        std::queue<size_t> active_queue;
        std::vector<bool> in_queue(worklist.size(), false);
        const auto& recursive_nodes = tctx.context().state().get_recursive_io_input_descs();

        // 伝播の起点をキューに初期登録
        for (size_t i = 0; i < worklist.size(); ++i) {
            auto& work = worklist[i];

            // 葉ノード、ルートノード、または事前計算された再帰ノードを起点とする
            bool is_boundary = (work.parents.empty() == (work.children == nullptr));
            if (is_boundary || recursive_nodes.contains(work.type)) {
                active_queue.push(i);
                in_queue[i] = true;
            }
        }

        // 幅優先での状態伝播 (閉路が存在しても、状態が変化しなくなった時点で自然に停止する)
        while (!active_queue.empty()) {
            size_t curr_idx = active_queue.front();
            active_queue.pop();
            in_queue[curr_idx] = false;  // キューから取り出されたためフラグを下ろす

            auto& curr_work = worklist[curr_idx];
            const auto current_desc = curr_work.io_input_desc;  // 現在の状態のスナップショット

            // 親ノードへの伝播 (相互依存関係の解決)
            for (ebm::TypeRef parent : curr_work.parents) {
                auto it = indices.find(parent);
                if (it != indices.end()) {
                    size_t p_idx = it->second;
                    if (apply_propagation(worklist[p_idx].io_input_desc, current_desc)) {
                        worklist[p_idx].changed = true;
                        if (!in_queue[p_idx]) {
                            active_queue.push(p_idx);
                            in_queue[p_idx] = true;
                        }
                    }
                }
            }

            // 子ノードへの伝播
            if (curr_work.children) {
                for (ebm::TypeRef child : *curr_work.children) {
                    auto it = indices.find(child);
                    if (it != indices.end()) {
                        size_t c_idx = it->second;
                        if (apply_propagation(worklist[c_idx].io_input_desc, current_desc)) {
                            worklist[c_idx].changed = true;
                            if (!in_queue[c_idx]) {
                                active_queue.push(c_idx);
                                in_queue[c_idx] = true;
                            }
                        }
                    }
                }
            }
        }

        for (auto& work : worklist) {
            if (ebmgen::verbose_error) {
                print_if_verbose("Propagation root: ", get_id(work.type), " ", to_string(work.kind), "\n");
                work.io_input_desc.visit([](auto&&, const char* name, auto&& value) {
                    print_if_verbose("  ", name, ": ", value, "\n");
                });
                if (work.children) {
                    print_if_verbose("  Children: ");
                    for (auto& child : *work.children) {
                        print_if_verbose(get_id(child), " ");
                    }
                    print_if_verbose("\n");
                }
                if (!work.parents.empty()) {
                    print_if_verbose("  Parents: ");
                    for (auto& parent : work.parents) {
                        print_if_verbose(get_id(parent), " ");
                    }
                    print_if_verbose("\n");
                }
            }
            if (work.changed) {
                MAYBE_VOID(_, rewrite_input_type_with_io_desc(tctx.context(), work.type, work.kind == ebm::TypeKind::ENCODER_INPUT, work.io_input_desc));
            }
        }

        return {};
    }

}  // namespace ebmgen
