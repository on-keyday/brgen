/*license*/
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/converter.hpp"
#include "transform.hpp"
#include "../convert/helper.hpp"
#include <testutil/timer.h>
#include <cstddef>
#include <ranges>
#include "../converter.hpp"

namespace ebmgen {

    struct SizeofResult {
        std::uint64_t size = 0;
        bool fixed = false;
        bool primitive = false;
    };

    expected<std::optional<SizeofResult>> sizeof_type(TransformContext& tctx, ebm::TypeRef type) {
        MAYBE(field_type, tctx.type_repository().get(type));
        if (auto size = field_type.body.size()) {
            return SizeofResult{size->value(), true, true};
        }
        else if (auto base_type = field_type.body.base_type(); base_type && !is_nil(*base_type)) {
            return sizeof_type(tctx, *base_type);
        }
        else if (auto desc = field_type.body.variant_desc(); desc) {  // for VARIANT
            std::uint64_t size = 0;
            bool primitive = false;
            for (const auto& member : desc->members.container) {
                MAYBE(member_size, sizeof_type(tctx, member));
                if (member_size) {
                    size = std::max(size, member_size->size);
                    primitive = primitive || member_size->primitive;
                }
                else {
                    return std::nullopt;
                }
            }
            return SizeofResult{size, false, primitive};
        }
        else if (auto id = field_type.body.id()) {
            MAYBE(stmt, tctx.statement_repository().get(from_weak(*id)));
            if (auto decl = stmt.body.struct_decl()) {
                if (auto size = decl->size()) {
                    if (size->unit == ebm::SizeUnit::BIT_FIXED) {
                        return SizeofResult{size->size()->value(), true, false};
                    }
                    else if (size->unit == ebm::SizeUnit::BYTE_FIXED) {
                        return SizeofResult{size->size()->value() * 8, true, false};
                    }
                }
            }
        }
        return std::nullopt;
    }

    expected<ebm::ExpressionRef> get_self(TransformContext& tctx, ebm::StatementRef stmt_ref) {
        auto& ctx = tctx.context();
        MAYBE(struct_stmt, tctx.statement_repository().get(stmt_ref));
        MAYBE(struct_decl, struct_stmt.body.struct_decl());
        ebm::TypeBody body;
        body.kind = struct_decl.is_recursive() ? ebm::TypeKind::RECURSIVE_STRUCT : ebm::TypeKind::STRUCT;
        body.id(to_weak(stmt_ref));
        EBMA_ADD_TYPE(self_type_ref, std::move(body));
        EBM_SELF(self_expr, self_type_ref);
        return self_expr;
    }

    expected<std::pair<ebm::StatementRef, ebm::StatementRef>> derive_composite_accessor(
        TransformContext& tctx, ebm::StatementRef composite_ref,
        ebm::TypeRef composite_type,
        ebm::StatementRef field_ref,
        ebm::IdentifierRef field_name_ref,
        ebm::StatementRef parent_ref,
        size_t total_size, size_t current_size, size_t offset) {
        auto& ctx = tctx.context();
        ebm::FunctionDecl getter_decl, setter_decl;
        MAYBE(getter_id, tctx.context().repository().new_statement_id());
        MAYBE(setter_id, tctx.context().repository().new_statement_id());
        getter_decl.name = field_name_ref;
        setter_decl.name = field_name_ref;
        getter_decl.parent_format = to_weak(parent_ref);
        setter_decl.parent_format = to_weak(parent_ref);
        getter_decl.kind = ebm::FunctionKind::COMPOSITE_GETTER;
        setter_decl.kind = ebm::FunctionKind::COMPOSITE_SETTER;
        MAYBE(field_self_id, tctx.context().state().get_self_ref_for_id(field_ref));
        MAYBE(original_field, tctx.expression_repository().get(field_self_id));
        // MAYBE(base_, self_expr.body.base());  // must be MEMBER_ACCESS
        auto original_field_type = original_field.body.type;
        // auto base = base_;  // copy to avoid memory relocation
        MAYBE(self_, get_self(tctx, parent_ref));
        EBM_IDENTIFIER(composite_ident, composite_ref, composite_type);
        EBM_MEMBER_ACCESS(composite_expr, composite_type, self_, composite_ident);
        // EBMU_INT_LITERAL(offset_expr, offset);
        // EBMU_INT_LITERAL(size_expr, current_size);
        // EBMU_INT_LITERAL(total_size_expr, total_size);
        EBM_DEFINE_PARAMETER(input_param, {}, original_field_type, false);
        append(setter_decl.params, input_param_def);
        MAYBE(setter_return_type, get_single_type(ebm::TypeKind::PROPERTY_SETTER_RETURN, ctx));
        setter_decl.return_type = setter_return_type;
        getter_decl.return_type = original_field_type;

        MAYBE(original_field_type_decl, tctx.type_repository().get(original_field_type));
        auto src_type = original_field_type;
        if (original_field_type_decl.body.kind == ebm::TypeKind::VARIANT ||
            original_field_type_decl.body.kind == ebm::TypeKind::STRUCT) {  // TODO: map to each type
            EBMU_UINT_TYPE(temp_type, current_size);
            src_type = temp_type;
        }
        MAYBE(mask, get_max_value_expr(ctx, src_type));

        // getter
        // from 0x301f, 1 bit at offset 3 is 1 (big endian model)
        // shift_right = total_size - (offset + current_size)
        // mask = ((1 << current_size) - 1) == src_type max value
        // value = (composite_expr >> shift_right) & mask
        EBMU_INT_LITERAL(shift_right, total_size - (offset + current_size));
        EBM_BINARY_OP(shifted, ebm::BinaryOp::right_shift, composite_type, composite_expr, shift_right);
        EBM_BINARY_OP(masked, ebm::BinaryOp::bit_and, composite_type, shifted, mask);
        EBM_CAST(getter_expr, original_field_type, composite_type, masked);
        EBM_RETURN(getter_return, getter_expr, getter_id);
        getter_decl.body = getter_return;

        // setter
        // to 0x301f, set 1 bit at offset 3 to 0 (then 0x201f)
        // shift_left = total_size - (offset + current_size)
        // clear_mask = ((1 << current_size) - 1)
        // clear_set_mask = ~(clear_mask << shift_left)
        // cleared_composite = composite_expr & bit_mask
        // set_value = (input_param & clear_mask) << shift_left
        // updated_composite = cleared_composite | set_value

        auto shift_left = shift_right;  // same value
        EBM_BINARY_OP(clear_mask, ebm::BinaryOp::left_shift, composite_type, mask, shift_left);
        EBM_UNARY_OP(clear_set_mask, ebm::UnaryOp::bit_not, composite_type, clear_mask);
        EBM_BINARY_OP(cleared_composite, ebm::BinaryOp::bit_and, composite_type, composite_expr, clear_set_mask);
        EBM_CAST(input_casted, composite_type, original_field_type, input_param);
        EBM_BINARY_OP(input_masked, ebm::BinaryOp::bit_and, composite_type, input_casted, mask);
        EBM_BINARY_OP(set_value, ebm::BinaryOp::left_shift, composite_type, input_masked, shift_left);
        EBM_BINARY_OP(updated_composite, ebm::BinaryOp::bit_or, composite_type, cleared_composite, set_value);

        EBM_ASSIGNMENT(assigned, composite_expr, updated_composite);
        ebm::Block setter_block;
        append(setter_block, assigned);
        EBM_SETTER_STATUS(status_ok, setter_return_type, ebm::SetterStatus::SUCCESS);
        EBM_RETURN(setter_return, status_ok, setter_id);
        append(setter_block, setter_return);
        EBM_BLOCK(setter_body, std::move(setter_block));
        setter_decl.body = setter_body;

        ebm::StatementBody getter_body{.kind = ebm::StatementKind::FUNCTION_DECL};
        getter_body.func_decl(std::move(getter_decl));
        EBMA_ADD_STATEMENT(getter_stmt, getter_id, std::move(getter_body));
        ebm::StatementBody setter_body_{.kind = ebm::StatementKind::FUNCTION_DECL};
        setter_body_.func_decl(std::move(setter_decl));
        EBMA_ADD_STATEMENT(setter_stmt, setter_id, std::move(setter_body_));

        return std::pair{getter_stmt, setter_stmt};
    }

    ebm::CompositeFieldKind determine_composite_field_kind(std::vector<SizeofResult>& fixed_map) {
        ebm::CompositeFieldKind kind = ebm::CompositeFieldKind::BULK_PRIMITIVE;
        bool is_composite = false;
        size_t i = 0;
        for (; i < fixed_map.size(); i++) {
            if (!fixed_map[i].fixed) {
                kind = is_composite ? ebm::CompositeFieldKind::PREFIXED_UNION_COMPOSITE : ebm::CompositeFieldKind::PREFIXED_UNION_PRIMITIVE;
                i++;
                break;
            }
            if (!fixed_map[i].primitive) {
                kind = ebm::CompositeFieldKind::BULK_COMPOSITE;
                is_composite = true;
            }
        }
        if (i == fixed_map.size()) {
            return kind;
        }
        return is_composite ? ebm::CompositeFieldKind::SANDWICHED_UNION_COMPOSITE : ebm::CompositeFieldKind::SANDWICHED_UNION_PRIMITIVE;
    }

    expected<void> merge_bit_field(TransformContext& tctx) {
        auto& ctx = tctx.context();
        auto& all_statements = tctx.statement_repository().get_all();
        size_t current_size = all_statements.size();
        for (size_t j = 0; j < current_size; j++) {
            if (auto struct_decl = all_statements[j].body.struct_decl()) {
                auto fields = struct_decl->fields.container;  // copy to avoid relocation
                std::vector<std::pair<size_t, std::optional<SizeofResult>>> sized_fields;
                std::vector<size_t> not_added_index;
                size_t added = 0;
                for (size_t index = 0; index < fields.size(); index++) {
                    auto& field = fields[index];
                    MAYBE(field_stmt, tctx.statement_repository().get(field));
                    if (auto field_decl = field_stmt.body.field_decl()) {
                        MAYBE(size, sizeof_type(tctx, field_decl->field_type));
                        sized_fields.push_back({index, size});
                        added++;
                    }
                    else {
                        not_added_index.push_back(index);
                    }
                }
                std::vector<std::pair<std::optional<size_t>, std::vector<std::pair<size_t /*index*/, SizeofResult /*size*/>>>> merged;
                for (auto& [index, size] : sized_fields) {
                    if (!size) {
                        merged.push_back({std::nullopt, {{index, {0, false}}}});
                        continue;
                    }
                    if (merged.empty()) {
                        merged.push_back({size->size, {{index, *size}}});
                        continue;
                    }
                    auto& [last_size, last_indexes] = merged.back();
                    if (!last_size) {
                        merged.push_back({size->size, {{index, *size}}});
                        continue;
                    }
                    if (*last_size % 8 != 0) {
                        last_indexes.push_back({index, *size});
                        *last_size += size->size;
                        continue;
                    }
                    auto is_common = [](size_t size) {
                        return size == 8 || size == 16 || size == 32 || size == 64;
                    };
                    if (!is_common(*last_size)) {
                        last_indexes.push_back({index, *size});
                        *last_size += size->size;
                        continue;
                    }
                    if (!is_common(size->size) && is_common(*last_size + size->size)) {
                        last_indexes.push_back({index, *size});
                        *last_size += size->size;
                        continue;
                    }
                    merged.push_back({size->size, {{index, *size}}});
                }
                if (merged.size() == added) {
                    continue;
                }
                print_if_verbose("Merging bit fields\n");
                ebm::Block block;
                for (size_t i = 0; i < merged.size(); ++i) {
                    auto& [size, indexes] = merged[i];
                    print_if_verbose("  - Merging ", indexes.size(), " fields");
                    if (size) {
                        print_if_verbose(" with total size ", *size, " bit");
                        if (*size % 8 == 0) {
                            print_if_verbose(" = ", *size / 8, " byte");
                        }
                        print_if_verbose("\n");
                    }
                    else {
                        print_if_verbose(" with unknown size\n");
                    }
                    if (indexes.size() == 1) {
                        append(block, fields[indexes[0].first]);
                    }
                    else {
                        ebm::CompositeFieldDecl comp_decl;
                        std::vector<SizeofResult> fixed_map;
                        for (auto& idx : indexes) {
                            append(comp_decl.fields, fields[idx.first]);
                            fixed_map.push_back(idx.second);
                        }
                        comp_decl.kind = determine_composite_field_kind(fixed_map);
                        EBMU_UINT_TYPE(composite_type, *size);
                        comp_decl.composite_type = composite_type;
                        ebm::StatementBody body;
                        body.kind = ebm::StatementKind::COMPOSITE_FIELD_DECL;
                        body.composite_field_decl(std::move(comp_decl));
                        EBMA_ADD_STATEMENT(comp_stmt, std::move(body));
                        size_t offset = 0;
                        for (auto& idx : indexes) {
                            std::pair<ebm::StatementRef, ebm::StatementRef> getter_setter;
                            {
                                MAYBE(field_stmt, tctx.statement_repository().get(fields[idx.first]));
                                MAYBE(field_decl, field_stmt.body.field_decl());
                                MAYBE(getter_setter_, derive_composite_accessor(
                                                          tctx,
                                                          comp_stmt,
                                                          composite_type,
                                                          fields[idx.first],
                                                          field_decl.name,
                                                          all_statements[j].id,
                                                          *size,
                                                          idx.second.size,
                                                          offset /* offset */));
                                getter_setter = getter_setter_;
                                offset += idx.second.size;
                            }
                            // refetch because of memory relocation
                            MAYBE(field_stmt, tctx.statement_repository().get(fields[idx.first]));
                            MAYBE(field_decl, field_stmt.body.field_decl());
                            field_decl.inner_composite(true);
                            field_decl.composite_field(to_weak(comp_stmt));
                            field_decl.composite_getter(ebm::LoweredStatementRef{getter_setter.first});
                            field_decl.composite_setter(ebm::LoweredStatementRef{getter_setter.second});
                        }
                        append(block, comp_stmt);
                    }
                }
                for (auto& not_added : not_added_index) {
                    append(block, fields[not_added]);
                }
                all_statements[j].body.struct_decl()->fields = std::move(block);
            }
        }
        return {};
    }
}  // namespace ebmgen
