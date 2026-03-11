/*license*/
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/converter.hpp"
#include "ebmgen/mapping.hpp"
#include "transform.hpp"
#include <cstddef>
#include <unordered_set>
#include <testutil/timer.h>

namespace ebmgen {

    using InverseRefs = std::unordered_map<size_t, std::vector<ebm::AnyRef>>;

    EBMProxy to_mapping_table(TransformContext& ctx) {
        return EBMProxy(ctx.statement_repository().get_all(),
                        ctx.expression_repository().get_all(),
                        ctx.type_repository().get_all(),
                        ctx.identifier_repository().get_all(),
                        ctx.string_repository().get_all(),
                        ctx.alias_vector(),
                        ctx.debug_locations());
    }

    expected<InverseRefs> mark_and_sweep(TransformContext& ctx, std::function<void(const char*)> timer) {
        MappingTable table{to_mapping_table(ctx), lazy_init};
        table.build_maps(mapping::BuildMapOption::NONE);
        std::unordered_set<size_t> reachable;
        InverseRefs inverse_refs;  // ref -> item.id
        auto search = [&](auto&& search, const auto& item) -> void {
            item.body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                if constexpr (AnyRef<decltype(val)>) {
                    if (!is_nil(val)) {
                        // if newly reachable, mark and search further
                        if (reachable.insert(get_id(val)).second) {
                            auto object = table.get_object(val);
                            std::visit(
                                [&](auto&& obj) -> void {
                                    using T = std::decay_t<decltype(obj)>;
                                    if constexpr (std::is_pointer_v<T>) {
                                        search(search, *obj);
                                    }
                                },
                                object);
                        }
                        inverse_refs[get_id(val)].push_back(to_any_ref(item.id));
                    }
                }
                else
                    VISITOR_RECURSE_CONTAINER(visitor, name, val)
                else VISITOR_RECURSE(visitor, name, val)
            });
        };
        // root is item_id == 1
        reachable.insert(1);
        {
            ebm::StatementRef root{1};
            auto object = table.get_object(root);
            std::visit(
                [&](auto&& obj) -> void {
                    using T = std::decay_t<decltype(obj)>;
                    if constexpr (std::is_pointer_v<T>) {
                        search(search, *obj);
                    }
                },
                object);
        }
        // also, reachable from file names
        for (const auto& d : ctx.file_names()) {
            reachable.insert(get_id(d));
            inverse_refs[get_id(d)].push_back(ebm::AnyRef{1});  // used from root
        }
        if (timer) {
            timer("mark phase");
        }
        // sweep
        size_t remove_count = 0;
        auto remove = [&](auto& rem) {
            std::decay_t<decltype(rem)> new_vec;
            new_vec.reserve(rem.size());
            for (auto& r : rem) {
                if (reachable.find(get_id(r.id)) == reachable.end()) {
                    remove_count++;
                    if (ebmgen::verbose_error) {
                        print_if_verbose("Removing unused item: ", get_id(r.id));
                        if constexpr (has_body_kind<decltype(r)>) {
                            print_if_verbose("(", to_string(r.body.kind), ")");
                        }
                        print_if_verbose("\n");
                    }
                    continue;
                }
                new_vec.push_back(std::move(r));
            }
            rem = std::move(new_vec);
        };
        remove(ctx.statement_repository().get_all());
        remove(ctx.identifier_repository().get_all());
        remove(ctx.type_repository().get_all());
        remove(ctx.string_repository().get_all());
        remove(ctx.expression_repository().get_all());
        std::erase_if(ctx.alias_vector(), [&](const auto& alias) {
            return reachable.find(get_id(alias.from)) == reachable.end() ||
                   reachable.find(get_id(alias.to)) == reachable.end();
        });
        print_if_verbose("Total removed unused items: ", remove_count, "\n");
        if (timer) {
            timer("sweep phase");
        }
        return inverse_refs;
    }

    expected<void> remove_unused_object(TransformContext& ctx, std::function<void(const char*)> timer) {
        MAYBE(max_id, ctx.max_id());
        futils::test::Timer t;
        MAYBE(inverse_refs, mark_and_sweep(ctx, timer));

        print_if_verbose("Removed unused items in ", t.next_step<std::chrono::microseconds>(), "\n");
        std::vector<std::tuple<ebm::AnyRef, size_t>> most_used;
        for (const auto& [id, refs] : inverse_refs) {
            most_used.emplace_back(ebm::AnyRef{id}, refs.size());
        }
        std::stable_sort(most_used.begin(), most_used.end(), [](const auto& a, const auto& b) {
            return std::get<1>(a) > std::get<1>(b);
        });
        std::unordered_map<size_t, ebm::AnyRef> old_to_new;
        ctx.set_max_id(0);  // reset
        for (auto& mapping : most_used) {
            MAYBE(new_id, ctx.new_id());
            old_to_new[get_id(std::get<0>(mapping))] = new_id;
        }
        MAYBE(entry_id, ctx.new_id());
        old_to_new[1] = entry_id;
        auto remap = [&](auto& vec) {
            t.reset();
            std::vector<std::pair<ebm::AnyRef, size_t>> id_sort;
            size_t index = 0;
            for (auto& item : vec) {
                if (auto it = old_to_new.find(get_id(item.id)); it != old_to_new.end()) {
                    item.id.id = it->second.id;
                }
                id_sort.push_back({to_any_ref(item.id), index});
                index++;
                item.body.visit([&](auto&& visitor, const char* name, auto&& val, std::optional<size_t> index = std::nullopt) -> void {
                    if constexpr (AnyRef<decltype(val)>) {
                        if (!is_nil(val)) {
                            auto it = old_to_new.find(get_id(val));
                            if (it != old_to_new.end()) {
                                val.id = it->second.id;
                            }
                        }
                    }
                    else if constexpr (is_container<decltype(val)>) {
                        for (size_t i = 0; i < val.container.size(); ++i) {
                            visitor(visitor, name, val.container[i], i);
                        }
                    }
                    else
                        VISITOR_RECURSE(visitor, name, val)
                });
            }
            print_if_verbose("Remap ", vec.size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
            t.reset();
            std::decay_t<decltype(vec)> new_vec;
            new_vec.resize(vec.size());
            std::sort(id_sort.begin(), id_sort.end(), [](const auto& a, const auto& b) {
                return get_id(a.first) < get_id(b.first);
            });
            size_t i = 0;
            for (auto& v : id_sort) {
                new_vec[i++] = std::move(vec[v.second]);
            }
            vec = std::move(new_vec);
            print_if_verbose("Sort ", vec.size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        };
        remap(ctx.statement_repository().get_all());
        remap(ctx.identifier_repository().get_all());
        remap(ctx.type_repository().get_all());
        remap(ctx.string_repository().get_all());
        remap(ctx.expression_repository().get_all());
        t.reset();
        for (auto& alias : ctx.alias_vector()) {
            if (auto it = old_to_new.find(get_id(alias.from)); it != old_to_new.end()) {
                alias.from.id = it->second.id;
            }
            if (auto it = old_to_new.find(get_id(alias.to)); it != old_to_new.end()) {
                alias.to.id = it->second.id;
            }
        }
        print_if_verbose("Remap ", ctx.alias_vector().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        t.reset();
        std::sort(ctx.alias_vector().begin(), ctx.alias_vector().end(), [](const auto& a, const auto& b) {
            return get_id(a.from) < get_id(b.from);
        });
        print_if_verbose("Sort ", ctx.alias_vector().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        t.reset();
        size_t removed_debug = 0;
        std::erase_if(ctx.debug_locations(), [&](auto& d) {
            if (!old_to_new.contains(get_id(d.ident))) {
                removed_debug++;
                return true;
            }
            return false;
        });
        print_if_verbose("Removed ", removed_debug, " unreferenced debug information in ", t.delta<std::chrono::microseconds>(), "\n");
        for (auto& loc : ctx.debug_locations()) {
            if (auto it = old_to_new.find(get_id(loc.ident)); it != old_to_new.end()) {
                loc.ident.id = it->second.id;
            }
        }
        print_if_verbose("Remap ", ctx.debug_locations().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        t.reset();
        for (auto& f : ctx.file_names()) {
            if (auto it = old_to_new.find(get_id(f)); it != old_to_new.end()) {
                f.id = it->second.id;
            }
        }
        if (timer) {
            timer("remap ids");
        }
        return {};
    }

}  // namespace ebmgen
