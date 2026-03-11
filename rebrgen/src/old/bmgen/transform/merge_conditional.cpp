/*license*/
#include <bmgen/internal.hpp>

namespace rebgn {
    struct MergeCtx {
        std::unordered_map<std::string, Storages> type_to_storage;

        std::unordered_map<ObjectID, std::pair<std::string, std::vector<Varint>>> merged_conditional_fields;
        std::unordered_map<ObjectID, std::vector<ObjectID>> properties_to_merged;

        std::unordered_map<ObjectID, std::vector<std::pair<ObjectID /*new id*/, ObjectID /*merged conditional field*/>>> conditional_properties;

        std::unordered_set<ObjectID> reached;
        std::unordered_set<ObjectID> exists;
    };

    Error merge_conditional_inner(Module& m, MergeCtx& ctx, size_t start, AbstractOp end_op) {
        if (ctx.reached.find(start) != ctx.reached.end()) {
            return none;
        }
        ctx.reached.insert(start);
        std::unordered_map<ObjectID, std::unordered_map<std::string, std::vector<Varint>>> conditional_fields;
        std::unordered_set<ObjectID> exists;
        for (size_t i = start; m.code[i].op != end_op; i++) {
            auto& c = m.code[i];
            if (c.op == AbstractOp::CONDITIONAL_FIELD) {
                auto parent = c.belong().value().value();
                auto child = c.right_ref().value().value();
                auto target = c.ident().value();
                auto idx = m.ident_index_table[child];
                if (m.code[idx].op == AbstractOp::DEFINE_FIELD) {
                    auto type = m.code[idx].type().value().ref.value();
                    auto found = m.storage_key_table_rev.find(type);
                    if (found == m.storage_key_table_rev.end()) {
                        return error("Invalid storage key");
                    }
                    auto& key = found->second;
                    auto& map = conditional_fields[parent];
                    map[key].push_back(target);
                }
                else if (m.code[idx].op == AbstractOp::DEFINE_PROPERTY) {
                    auto err = merge_conditional_inner(m, ctx, idx + 1, AbstractOp::END_PROPERTY);
                    if (err) {
                        return err;
                    }
                    auto& merged = ctx.properties_to_merged[child];
                    for (auto& mc : merged) {
                        auto& mf = ctx.merged_conditional_fields[mc];
                        auto& map = conditional_fields[parent];
                        auto& key = mf.first;
                        auto new_id = m.new_id(nullptr);
                        if (!new_id) {
                            return new_id.error();
                        }
                        ctx.conditional_properties[target.value()].push_back({new_id->value(), mc});
                        map[key].push_back(*new_id);
                    }
                }
            }
            else if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
                exists.insert(c.ident().value().value());
                ctx.exists.insert(c.ident().value().value());
                auto param = c.param().value();
                auto found = m.storage_key_table_rev.find(c.type().value().ref.value());
                if (found == m.storage_key_table_rev.end()) {
                    return error("Invalid storage key");
                }
                auto& key = found->second;
                for (auto& p : param.refs) {
                    auto& code = m.code[m.ident_index_table[p.value()]];
                    if (auto found = ctx.conditional_properties.find(code.ident()->value()); found != ctx.conditional_properties.end()) {
                        for (auto& mf : found->second) {
                            if (auto found2 = ctx.merged_conditional_fields.find(mf.second); found2 != ctx.merged_conditional_fields.end()) {
                                if (found2->second.first == key) {
                                    p = *varint(mf.first);
                                    break;
                                }
                            }
                        }
                    }
                }
                c.param(std::move(param));
            }
        }
        for (auto& cfield : conditional_fields) {
            for (auto& [key, value] : cfield.second) {
                bool exist = false;
                for (auto& e : exists) {
                    auto& c = m.code[m.ident_index_table[e]];
                    auto found = m.storage_key_table_rev.find(c.type().value().ref.value());
                    if (found == m.storage_key_table_rev.end()) {
                        return error("Invalid storage key");
                    }
                    auto& common_type_key = found->second;
                    auto param = c.param().value().refs;
                    if (common_type_key == key && std::ranges::equal(param, value, [](auto& a, auto& b) {
                            return a.value() == b.value();
                        })) {
                        c.merge_mode(MergeMode::STRICT_COMMON_TYPE);
                        exist = true;
                        ctx.merged_conditional_fields[e] = {key, std::move(value)};
                        ctx.properties_to_merged[cfield.first].push_back(e);
                        break;
                    }
                }
                if (exist) {
                    continue;
                }
                auto new_id = m.new_id(nullptr);
                if (!new_id) {
                    return new_id.error();
                }
                ctx.merged_conditional_fields[new_id->value()] = {key, std::move(value)};
                ctx.properties_to_merged[cfield.first].push_back(new_id->value());
            }
        }
        return none;
    }

    Error merge_conditional_field(Module& m) {
        MergeCtx ctx;
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DECLARE_PROPERTY) {
                auto index = m.ident_index_table[m.code[i].ref().value().value()];
                auto err = merge_conditional_inner(m, ctx, index + 1, AbstractOp::END_PROPERTY);
                if (err) {
                    return err;
                }
            }
        }
        std::vector<Code> rebound;
        std::optional<Varint> target;
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_PROPERTY) {
                target = m.code[i].ident();
            }
            else if (m.code[i].op == AbstractOp::END_PROPERTY) {
                if (!target) {
                    return error("Invalid target");
                }
                auto found = ctx.properties_to_merged.find(target->value());
                if (found != ctx.properties_to_merged.end()) {
                    for (auto& mr : found->second) {
                        if (ctx.exists.find(mr) != ctx.exists.end()) {
                            continue;
                        }
                        auto& merged = ctx.merged_conditional_fields[mr];
                        Param param;
                        param.refs = std::move(merged.second);
                        param.len = *varint(param.refs.size());
                        auto found2 = m.storage_key_table.find(merged.first);
                        if (found2 == m.storage_key_table.end()) {
                            return error("Invalid storage key");
                        }
                        rebound.push_back(make_code(AbstractOp::MERGED_CONDITIONAL_FIELD, [&](auto& c) {
                            c.ident(*varint(mr));
                            c.type(StorageRef{.ref = *varint(found2->second)});
                            c.param(std::move(param));
                            c.belong(*target);
                            c.merge_mode(MergeMode::STRICT_TYPE);
                        }));
                    }
                }
                target.reset();
            }
            else if (m.code[i].op == AbstractOp::CONDITIONAL_FIELD) {
                if (auto found = ctx.conditional_properties.find(m.code[i].ident()->value());
                    found != ctx.conditional_properties.end()) {
                    auto left_ref = m.code[i].left_ref().value();  // condition expr
                    auto belong = m.code[i].belong().value();
                    for (auto& c : found->second) {
                        rebound.push_back(make_code(AbstractOp::CONDITIONAL_PROPERTY, [&](auto& code) {
                            code.ident(*varint(c.first));
                            code.left_ref(left_ref);
                            code.right_ref(*varint(c.second));
                            code.belong(belong);
                        }));
                    }
                    continue;  // remove original conditional field
                }
            }
            rebound.push_back(std::move(m.code[i]));
        }
        m.code = std::move(rebound);
        return none;
    }

}  // namespace rebgn
