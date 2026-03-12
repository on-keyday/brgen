/*license*/

#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/converter.hpp"
#include "helper.hpp"
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace ebmgen {
    struct DerivedTypeInfo {
        ebm::ExpressionRef getter_cond;
        ebm::ExpressionRef setter_cond;
        std::optional<ebm::StatementRef> field;  // field or property
    };

    struct WithoutFieldConds {
        ebm::ExpressionRef getter_cond;
        ebm::ExpressionRef setter_cond;
    };

    expected<std::optional<ebm::TypeRef>> get_common_type(ConverterContext& ctx, ebm::TypeRef a, ebm::TypeRef b) {
        if (get_id(a) == get_id(b)) {
            return a;
        }
        MAYBE(type_A, ctx.repository().get_type(a));
        MAYBE(type_B, ctx.repository().get_type(b));
        if (type_A.body.kind != type_B.body.kind) {
            bool is_integral_A = type_A.body.kind == ebm::TypeKind::INT || type_A.body.kind == ebm::TypeKind::UINT || type_A.body.kind == ebm::TypeKind::USIZE;
            bool is_integral_B = type_B.body.kind == ebm::TypeKind::INT || type_B.body.kind == ebm::TypeKind::UINT || type_B.body.kind == ebm::TypeKind::USIZE;
            if (type_A.body.kind == ebm::TypeKind::USIZE && (type_B.body.kind == ebm::TypeKind::INT || type_B.body.kind == ebm::TypeKind::UINT)) {
                return a;
            }
            else if (type_B.body.kind == ebm::TypeKind::USIZE && (type_A.body.kind == ebm::TypeKind::INT || type_A.body.kind == ebm::TypeKind::UINT)) {
                return b;
            }
            if (is_integral_A && type_B.body.kind == ebm::TypeKind::VARIANT) {
                return a;
            }
            else if (is_integral_B && type_A.body.kind == ebm::TypeKind::VARIANT) {
                return b;
            }
            auto is_any_range = [&](ebm::Type& t) {
                return t.body.kind == ebm::TypeKind::RANGE && is_nil(*t.body.base_type());
            };
            if (is_any_range(type_A)) {
                return b;
            }
            else if (is_any_range(type_B)) {
                return a;
            }
            return std::nullopt;
        }
        switch (type_A.body.kind) {
            case ebm::TypeKind::INT:
            case ebm::TypeKind::UINT:
            case ebm::TypeKind::FLOAT: {
                auto sizeA = type_A.body.size();
                auto sizeB = type_B.body.size();
                if (sizeA->value() > sizeB->value()) {
                    return a;
                }
                return b;
            }
            default: {
                return std::nullopt;
            }
        }
    }

    expected<std::pair<WithoutFieldConds, std::vector<DerivedTypeInfo>>> convert_union_type_to_ebm(ConverterContext& ctx, ast::UnionType& union_type) {
        std::vector<DerivedTypeInfo> cases;
        for (auto& cand : union_type.candidates) {
            DerivedTypeInfo c;
            auto cond_locked = cand->cond.lock();
            if (cond_locked) {
                {
                    const auto _scope = ctx.state().set_current_generate_type(GenerateType::PropertyGetter);
                    EBMA_CONVERT_EXPRESSION(expr, cond_locked);
                    c.getter_cond = expr;
                }
                {
                    const auto _scope = ctx.state().set_current_generate_type(GenerateType::PropertySetter);
                    EBMA_CONVERT_EXPRESSION(expr, cond_locked);
                    c.setter_cond = expr;
                }
            }
            auto field = cand->field.lock();
            if (field) {
                EBMA_CONVERT_STATEMENT(stmt, field);
                c.field = stmt;
            }
            cases.push_back(c);
        }
        WithoutFieldConds base_cond;
        if (auto cond = union_type.cond.lock()) {
            {
                const auto _scope = ctx.state().set_current_generate_type(GenerateType::PropertyGetter);
                EBMA_CONVERT_EXPRESSION(expr, cond);
                base_cond.getter_cond = expr;
            }
            {
                const auto _scope = ctx.state().set_current_generate_type(GenerateType::PropertySetter);
                EBMA_CONVERT_EXPRESSION(expr, cond);
                base_cond.setter_cond = expr;
            }
        }
        return std::make_pair(std::move(base_cond), std::move(cases));
    }

    expected<void> map_field(ConverterContext& ctx, ebm::StatementRef field, ebm::PropertyDecl& decl, auto&& action) {
        if (decl.merge_mode == ebm::MergeMode::STRICT_TYPE) {
            action(field, decl);
        }
        else {
            MAYBE(derived_from, decl.derived_from());
            for (auto& m : derived_from.container) {
                MAYBE(child, ctx.repository().get_statement(m));
                MAYBE(property, child.body.property_decl());
                MAYBE_VOID(ok, map_field(ctx, child.id, property, action));
            }
        }
        return {};
    }

    constexpr auto without_field = 0;
    constexpr auto with_field = 1;

    using PropExprVec = std::vector<std::variant<WithoutFieldConds, ebm::PropertyMemberDecl>>;
    using MergedMap = std::unordered_map<std::uint64_t, PropExprVec>;
    struct DetectedTypes {
        std::vector<ebm::TypeRef> detected_types;

        MergedMap merged;
    };

    expected<DetectedTypes> detect_all_types(ConverterContext& ctx, std::vector<DerivedTypeInfo>& cases) {
        DetectedTypes result;
        // detect all types before merge
        for (auto& c : cases) {
            if (c.field) {
                MAYBE(field, ctx.repository().get_statement(*c.field));
                if (auto decl = field.body.field_decl()) {
                    if (result.merged.emplace(get_id(decl->field_type), PropExprVec{}).second) {
                        result.detected_types.push_back(decl->field_type);
                    }
                }
                else if (auto decl = field.body.property_decl()) {
                    MAYBE_VOID(ok, map_field(ctx, field.id, *decl, [&](ebm::StatementRef, ebm::PropertyDecl& decl) {
                                   if (result.merged.emplace(get_id(decl.property_type), PropExprVec{}).second) {
                                       result.detected_types.push_back(decl.property_type);
                                   }
                               }));
                }
                else {
                    return unexpect_error("Only field or property can be used in union type");
                }
            }
        }
        return result;
    }

    expected<void> merge_fields_per_type(ConverterContext& ctx, std::vector<DerivedTypeInfo>& cases, std::unordered_map<std::uint64_t, PropExprVec>& merged) {
        auto add_without_field = [&](PropExprVec& p, WithoutFieldConds conds) {
            p.push_back(conds);
        };
        for (auto& c : cases) {
            if (c.field) {
                MAYBE(field, ctx.repository().get_statement(*c.field));
                std::unordered_set<std::uint64_t> added;
                if (auto decl = field.body.field_decl()) {
                    auto id = get_id(decl->field_type);
                    added.insert(id);
                    merged[id].push_back(ebm::PropertyMemberDecl{
                        .setter_condition = c.setter_cond,
                        .getter_condition = c.getter_cond,
                        .field = *c.field,
                    });
                }
                else if (auto decl = field.body.property_decl()) {
                    auto id = get_id(decl->property_type);
                    MAYBE_VOID(ok, map_field(ctx, field.id, *decl, [&](ebm::StatementRef field, ebm::PropertyDecl& decl) {
                                   added.insert(id);
                                   merged[id].push_back(ebm::PropertyMemberDecl{
                                       .setter_condition = c.setter_cond,
                                       .getter_condition = c.getter_cond,
                                       .field = field,
                                   });
                               }));
                }
                else {
                    return unexpect_error("Only field or property can be used in union type");
                }
                for (auto& not_added : merged) {
                    if (added.contains(not_added.first)) {
                        continue;
                    }
                    add_without_field(not_added.second,
                                      WithoutFieldConds{
                                          .getter_cond = c.getter_cond,
                                          .setter_cond = c.setter_cond,
                                      });
                }
            }
            else {
                for (auto& a : merged) {
                    add_without_field(a.second,
                                      WithoutFieldConds{
                                          .getter_cond = c.getter_cond,
                                          .setter_cond = c.setter_cond,
                                      });
                }
            }
        }
        if (merged.size() == 0) {
            return unexpect_error("This is a bug: no type detected in union property");
        }
        size_t merged_size = merged.begin()->second.size();
        for (auto& a : merged) {
            if (a.second.size() != merged_size) {
                return unexpect_error("This is a bug: inconsistent merged size");
            }
        }
        return {};
    }

    expected<ebm::ExpressionRef> derive_cond(ConverterContext& ctx, ebm::Expressions& without_field_conds) {
        ebm::TypeRef common_type;
        for (auto& expr : without_field_conds.container) {
            MAYBE(got, ctx.repository().get_expression(expr));
            if (is_nil(common_type)) {
                common_type = got.body.type;
            }
            else {
                MAYBE(ct, get_common_type(ctx, common_type, got.body.type));
                if (!ct) {
                    MAYBE(prev, ctx.repository().get_type(common_type));
                    MAYBE(current, ctx.repository().get_type(got.body.type));
                    return unexpect_error("cannot get common type: {} vs {}", to_string(prev.body.kind), to_string(current.body.kind));
                }
                common_type = *ct;
            }
        }
        if (without_field_conds.container.size() == 0) {
            return unexpect_error("This is a bug: empty condition in strict type merge");
        }
        ebm::ExpressionRef cond;
        if (without_field_conds.container.size() == 1) {
            cond = without_field_conds.container[0];
        }
        else {
            EBM_OR_COND(or_, common_type, std::move(without_field_conds));
            cond = or_;
        }
        without_field_conds = {};
        return cond;
    }

    expected<void> strict_merge(ConverterContext& ctx, ebm::PropertyDecl& derive, WithoutFieldConds base_cond, ebm::TypeRef type, PropExprVec& vec) {
        derive.merge_mode = ebm::MergeMode::STRICT_TYPE;
        derive.property_type = type;
        derive.getter_condition = base_cond.getter_cond;
        derive.setter_condition = base_cond.setter_cond;
        ebm::Expressions without_field_getter_conds;
        ebm::Expressions without_field_setter_conds;
        for (auto& member : vec) {
            if (auto got = std::get_if<without_field>(&member)) {
                append(without_field_getter_conds, got->getter_cond);
                append(without_field_setter_conds, got->setter_cond);
                continue;
            }
            if (without_field_getter_conds.container.size() > 0) {
                MAYBE(getter_cond, derive_cond(ctx, without_field_getter_conds));
                MAYBE(setter_cond, derive_cond(ctx, without_field_setter_conds));
                ebm::PropertyMemberDecl m;
                m.getter_condition = getter_cond;
                m.setter_condition = setter_cond;
                EBM_PROPERTY_MEMBER_DECL(member_ref, std::move(m));
                append(derive.members, member_ref);
            }
            auto m = std::get<ebm::PropertyMemberDecl>(member);
            EBM_PROPERTY_MEMBER_DECL(member_ref, std::move(m));
            append(derive.members, member_ref);
        }
        return {};
    }

    using IndexClusterElement = std::pair<std::vector<size_t>, std::unordered_set<size_t>>;
    using IndexCluster = std::vector<IndexClusterElement>;

    expected<IndexCluster> clustering_properties(ConverterContext& ctx, std::vector<ebm::PropertyDecl>& properties) {
        std::map<std::uint64_t, std::vector<size_t>> edge;
        // insertion order is important, so use both vector and set
        IndexCluster cluster;
        for (size_t i = 0; i < properties.size(); i++) {
            for (size_t j = i + 1; j < properties.size(); j++) {
                MAYBE(common, get_common_type(ctx, properties[i].property_type, properties[j].property_type));
                if (common) {
                    edge[i].push_back(j);
                    edge[j].push_back(i);
                }
            }
        }
        for (size_t i = 0; i < properties.size(); i++) {
            if (i == 0) {
                cluster.push_back({{i}, {i}});
                continue;
            }
            // try connecting to existing cluster
            auto& edges = edge[i];
            size_t min_index = cluster.size();
            for (auto& e : edges) {
                if (e >= i) {
                    continue;
                }
                min_index = std::min(min_index, e);
            }
            if (min_index == cluster.size()) {
                // new cluster
                cluster.push_back({{i}, {i}});
                continue;
            }
            bool ok = false;
            for (auto& c : cluster) {
                if (c.second.contains(min_index)) {
                    c.first.push_back(i);
                    c.second.insert(i);
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                return unexpect_error("This is a bug: cluster not found");
            }
        }
        return cluster;
    }

    auto derive_variant(ConverterContext& ctx, auto&& arr, ebm::TypeRef common_type, auto&& get_type) -> expected<ebm::TypeRef> {
        ebm::Types types;
        for (auto& t : arr) {
            append(types, get_type(t));
        }
        ebm::TypeBody body;
        body.kind = ebm::TypeKind::VARIANT;
        ebm::VariantDesc desc;
        desc.common_type = common_type;
        desc.members = std::move(types);
        body.variant_desc(std::move(desc));
        EBMA_ADD_TYPE(c_type, std::move(body));
        return c_type;
    }

    expected<void> common_merge_members(ConverterContext& ctx, ebm::PropertyDecl& prop, IndexClusterElement* c, std::vector<ebm::PropertyDecl>* properties, DetectedTypes& original_merged) {
        auto total_size = original_merged.merged.begin()->second.size();
        ebm::Expressions without_field_getter_conds;
        ebm::Expressions without_field_setter_conds;
        for (size_t i = 0; i < total_size; i++) {
            std::optional<ebm::PropertyMemberDecl> decl;
            std::optional<WithoutFieldConds> expr;
            for (auto& type_ref : original_merged.detected_types) {
                if (c && properties) {
                    bool found = false;
                    for (auto index : c->first) {
                        if (get_id((*properties)[index].property_type) != get_id(type_ref)) {
                            continue;
                        }
                        found = true;
                    }
                    if (!found) {
                        continue;
                    }
                }
                auto& field = original_merged.merged[get_id(type_ref)][i];
                if (auto got = std::get_if<without_field>(&field)) {
                    if (expr) {
                        if (get_id(expr->getter_cond) != get_id(got->getter_cond) ||
                            get_id(expr->setter_cond) != get_id(got->setter_cond)) {
                            return unexpect_error("This is a bug: inconsistent without_field condition");
                        }
                    }
                    else {
                        expr = *got;
                    }
                    continue;
                }
                if (decl) {
                    return unexpect_error("This is a bug: multiple field found in common merge");
                }
                auto m = std::get<ebm::PropertyMemberDecl>(field);
                decl = m;
            }
            if (!decl && !expr) {
                return unexpect_error("This is a bug: no field or condition found in common merge");
            }
            if (!decl) {
                append(without_field_getter_conds, expr->getter_cond);
                append(without_field_setter_conds, expr->setter_cond);
                continue;
            }
            if (without_field_getter_conds.container.size() > 0) {
                MAYBE(getter_cond, derive_cond(ctx, without_field_getter_conds));
                MAYBE(setter_cond, derive_cond(ctx, without_field_setter_conds));
                ebm::PropertyMemberDecl m;
                m.getter_condition = getter_cond;
                m.setter_condition = setter_cond;
                EBM_PROPERTY_MEMBER_DECL(member_ref, std::move(m));
                append(prop.members, member_ref);
            }
            EBM_PROPERTY_MEMBER_DECL(member_ref, std::move(*decl));
            append(prop.members, member_ref);
        }
        return {};
    }

    expected<std::vector<ebm::PropertyDecl>> common_merge(ConverterContext& ctx, WithoutFieldConds base_cond, ebm::PropertyDecl& derive, IndexCluster& cluster, std::vector<ebm::PropertyDecl>& properties, DetectedTypes& original_merged) {
        std::vector<ebm::PropertyDecl> final_props;

        for (auto& c : cluster) {
            if (c.first.size() == 1) {
                final_props.push_back(std::move(properties[c.first[0]]));
                continue;
            }
            ebm::TypeRef common_type;
            for (auto& index : c.first) {
                if (is_nil(common_type)) {
                    common_type = properties[index].property_type;
                    continue;
                }
                MAYBE(detect, get_common_type(ctx, common_type, properties[index].property_type));
                if (!detect) {
                    return unexpect_error("no common type found for clustered types; THIS IS BUG!");
                }
                common_type = *detect;
            }
            auto c_type = derive_variant(ctx, c.first, common_type, [&](size_t index) {
                return properties[index].property_type;
            });
            if (!c_type) {
                return unexpect_error(std::move(c_type.error()));
            }
            ebm::PropertyDecl prop;
            prop.name = derive.name;
            prop.property_type = *c_type;
            prop.parent_format = derive.parent_format;
            prop.getter_condition = base_cond.getter_cond;
            prop.setter_condition = base_cond.setter_cond;
            prop.merge_mode = ebm::MergeMode::COMMON_TYPE;
            MAYBE_VOID(merge_members, common_merge_members(ctx, prop, &c, &properties, original_merged));
            ebm::Block derived_from;
            for (auto& index : c.first) {
                EBM_PROPERTY_DECL(p, std::move(properties[index]));
                append(derived_from, p);
            }
            prop.derived_from(std::move(derived_from));
            final_props.push_back(std::move(prop));
        }
        return final_props;
    }

    expected<void> uncommon_merge(ConverterContext& ctx, WithoutFieldConds base_cond, ebm::PropertyDecl& derive, std::vector<ebm::PropertyDecl>& final_props, DetectedTypes& original_merged) {
        auto c_type = derive_variant(ctx, final_props, {}, [&](auto& prop) {
            return prop.property_type;
        });
        if (!c_type) {
            return unexpect_error(std::move(c_type.error()));
        }
        ebm::PropertyDecl prop;
        prop.name = derive.name;
        prop.property_type = *c_type;
        prop.parent_format = derive.parent_format;
        prop.getter_condition = base_cond.getter_cond;
        prop.setter_condition = base_cond.setter_cond;
        prop.merge_mode = ebm::MergeMode::UNCOMMON_TYPE;
        MAYBE_VOID(merge_members, common_merge_members(ctx, prop, nullptr, nullptr, original_merged));
        ebm::Block derived_from;
        for (auto& f : final_props) {
            EBM_PROPERTY_DECL(pd, std::move(f));
            append(derived_from, pd);
        }
        prop.derived_from(std::move(derived_from));
        derive = std::move(prop);
        return {};
    }

    expected<void> derive_property_type(ConverterContext& ctx, ebm::PropertyDecl& derive, ast::UnionType& union_type) {
        MAYBE(union_data, convert_union_type_to_ebm(ctx, union_type));
        auto [base_cond, cases] = std::move(union_data);
        MAYBE(all_type, detect_all_types(ctx, cases));
        auto& merged_fields = all_type.merged;
        MAYBE_VOID(merge_field, merge_fields_per_type(ctx, cases, merged_fields));
        print_if_verbose("Merged ", merged_fields.size(), " types for property\n");
        if (merged_fields.size() == 1) {  // single strict type
            MAYBE(varint_, varint(merged_fields.begin()->first));
            MAYBE_VOID(s, strict_merge(ctx, derive, base_cond, ebm::TypeRef{varint_}, merged_fields.begin()->second));
            return {};
        }
        std::vector<ebm::PropertyDecl> properties;
        for (auto& ty : all_type.detected_types) {
            ebm::PropertyDecl prop;
            prop.name = derive.name;
            prop.parent_format = derive.parent_format;
            MAYBE_VOID(s, strict_merge(ctx, prop, base_cond, ty, merged_fields[get_id(ty)]));
            properties.push_back(std::move(prop));
        }
        MAYBE(cluster, clustering_properties(ctx, properties));
        MAYBE(final_props, common_merge(ctx, base_cond, derive, cluster, properties, all_type));
        if (final_props.size() == 1) {  // all types are merged into single common type
            derive = std::move(final_props[0]);
        }
        else {
            MAYBE_VOID(prop, uncommon_merge(ctx, base_cond, derive, final_props, all_type));
        }
        return {};
    }

    expected<ebm::StatementBody> StatementConverter::convert_property_decl(const std::shared_ptr<ast::Field>& node) {
        ebm::StatementBody body;
        MAYBE(union_type, ast::as<ast::UnionType>(node->field_type));
        body.kind = ebm::StatementKind::PROPERTY_DECL;
        ebm::PropertyDecl prop_decl;
        EBMA_ADD_IDENTIFIER(field_name_ref, node->ident->ident);
        prop_decl.name = field_name_ref;

        if (auto parent_member = node->belong.lock()) {
            EBMA_CONVERT_STATEMENT(statement_ref, parent_member);
            prop_decl.parent_format = to_weak(statement_ref);
        }

        MAYBE_VOID(derived, derive_property_type(ctx, prop_decl, union_type));

        body.property_decl(std::move(prop_decl));
        ctx.state().cache_type(node->field_type, prop_decl.property_type);
        return body;
    }
}  // namespace ebmgen
