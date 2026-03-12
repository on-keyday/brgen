/*license*/
#include "ebm/extended_binary_module.hpp"
#include "../access.hpp"
#include "../converter.hpp"
#include "../visitor/visitor.hpp"
#include "ebmgen/converter.hpp"
#include "ebmgen/mapping.hpp"
#include "to_mapping_table.hpp"
#include "ebmcodegen/stub/make_visitor.hpp"
#include "../convert/helper.hpp"

namespace ebmgen {
    struct ArrayLengthInfo {
        ebm::ExpressionRef array_field_expr;
        ebm::ExpressionRef length_field_expr;
        ebm::StatementRef array_field_ref;
        ebm::StatementRef length_field_ref;
        ebm::TypeRef vector_type;
        ebm::TypeRef length_type;
    };
    inline std::optional<ArrayLengthInfo> is_setter_target(TransformContext& wctx, const ebm::IOData& data) {
        if (is_nil(data.field)) {
            return std::nullopt;
        }
        auto length_field = access_field<"member.body.id.id">(wctx.context().repository(), data.size.ref());
        auto check_length_decl = access_field<"field_decl">(wctx.context().repository(), length_field);
        if (!check_length_decl) {
            return std::nullopt;
        }
        auto length_type = access_field<"field_type.instance">(wctx.context().repository(), check_length_decl);
        if (!length_type) {
            return std::nullopt;
        }
        // length must be int or uint
        if (length_type->body.kind != ebm::TypeKind::INT &&
            length_type->body.kind != ebm::TypeKind::UINT) {
            return std::nullopt;
        }
        auto array_field = access_field<"member.body.id.id">(wctx.context().repository(), data.target);
        auto check_array_decl = access_field<"field_decl">(wctx.context().repository(), array_field);
        if (!check_array_decl) {
            return std::nullopt;
        }
        auto array_type = access_field<"field_type.instance">(wctx.context().repository(), check_array_decl);
        if (!array_type) {
            return std::nullopt;
        }
        if (array_type->body.kind != ebm::TypeKind::VECTOR) {
            return std::nullopt;
        }
        return ArrayLengthInfo{
            data.target,
            *data.size.ref(),
            *array_field,
            *length_field,
            array_type->id,
            length_type->id,
        };
    }

    struct BranchInfo {
        ebm::StatementRef field_ref;
        ebm::StatementRef assign_ref;
        ebm::StatementRef parent_ref;
        ebm::StatementRef func_ref;
    };

    /*
    struct PropertySetterDetector {
        visitor::BaseVisitor& visitor;
        std::unordered_map<size_t, BranchInfo>& branch_info;
        std::optional<ebm::StatementRef> current_match_branch;

        expected<void> visit(visitor::Context_Statement_ASSIGNMENT& ctx) {
            auto target_id = ctx.get_field<"member.body.id.id">(ctx.target);
            auto target_field_kind = ctx.get_field<"field_decl.field_type.body.kind.optional">(target_id);
            if (target_field_kind != ebm::TypeKind::VECTOR) {
                return {};
            }
            if (current_match_branch) {
                branch_info[get_id(*target_id)] = BranchInfo{
                    .field_ref = *target_id,
                    .assign_ref = ctx.item_id,
                    .branch_ref = *current_match_branch,
                };
            }
            return {};
        }

        expected<void> visit(visitor::Context_Statement_MATCH_BRANCH& ctx) {
            current_match_branch = ctx.item_id;
            auto res = traverse_children<void>(*this, ctx);
            current_match_branch = std::nullopt;
            return res;
        }

        template <class Ctx>
        expected<void> visit(Ctx&& ctx) {
            if (ctx.context_name.contains("Type")) {
                return {};
            }
            if (ctx.is_before_or_after()) {
                return ebmcodegen::util::pass;
            }
            return traverse_children<void>(*this, std::forward<Ctx>(ctx));
        }
    };
    */

    expected<void> derive_array_setter(TransformContext& tctx) {
        auto& ctx = tctx.context();
        std::unordered_map<size_t, ArrayLengthInfo> array_length_setters;
        std::unordered_map<size_t, BranchInfo> branch_info;
        visitor::BaseVisitor visitor{to_mapping_table(tctx)};
        visitor.module_.build_maps(mapping::BuildMapOption::NONE);
        visitor::InitialContext ictx{.visitor = visitor};
        std::vector<ebm::StatementRef> parent_nodes;
        ebm::StatementRef func_ref;
        auto detector =
            ebmcodegen::util::make_visitor<void>(visitor)
                .name("PropertySetterDetector")
                .not_context("Type")
                .not_before_or_after()
                .on([&](auto&& self, visitor::Context_Statement& ctx) {
                    parent_nodes.push_back(is_nil(ctx.alias_ref) ? ctx.in.id : ctx.alias_ref);
                    auto defer = futils::helper::defer([&] { parent_nodes.pop_back(); });
                    return traverse_children<void>(self, ctx);
                })
                .on([&](auto&& self, visitor::Context_Statement_ASSIGNMENT& ctx) -> expected<void> {
                    auto target_id = ctx.get_field<"member.body.id.id">(ctx.target);
                    auto target_field_kind = ctx.get_field<"field_decl.field_type.body.kind.optional">(target_id);
                    if (target_field_kind != ebm::TypeKind::VECTOR) {
                        return {};
                    }
                    branch_info[get_id(ctx.item_id)] = BranchInfo{
                        .field_ref = *target_id,
                        .assign_ref = ctx.item_id,
                        .parent_ref = parent_nodes.size() >= 2 ? parent_nodes[parent_nodes.size() - 2] : ebm::StatementRef{0},
                        .func_ref = func_ref,
                    };
                    return {};
                })
                .on_default_traverse_children()
                .build();
        // visitor::PropertySetterDetector detector{visitor, branch_info};
        for (auto& stmts : tctx.statement_repository().get_all()) {
            if (auto w = stmts.body.write_data()) {
                if (auto info = is_setter_target(tctx, *w); info) {
                    array_length_setters[get_id(w->field)] = *info;
                }
            }
            if (auto func = stmts.body.func_decl(); func && func->kind == ebm::FunctionKind::PROPERTY_SETTER) {
                func_ref = stmts.id;
                MAYBE_VOID(_, ictx.visit(detector, func->body));
            }
        }
        size_t max_index = tctx.statement_repository().get_all().size();
        MAYBE(setter_return_type, get_single_type(ebm::TypeKind::PROPERTY_SETTER_RETURN, ctx));

        // WARNING: this modifies the statement repository while iterating, be careful with the order of operations and possible invalidation of references
        auto derive_setter = [&](ebm::StatementRef func_ref, ebm::StatementRef assign_ref, const ArrayLengthInfo& array_info, ebm::ExpressionRef value) -> expected<ebm::StatementRef> {
            // copy of value
            // constructs:
            // if (value.length > max_length) {
            //    return error("Array length exceeds maximum");
            // }
            MAYBE(max_value_expr, get_max_value_expr(ctx, array_info.length_type));
            EBMU_COUNTER_TYPE(usize_type);
            EBM_CAST(limit, usize_type, array_info.length_type, max_value_expr);
            EBM_ARRAY_SIZE(to_set_len, value);
            EBMU_BOOL_TYPE(bool_type);
            EBM_BINARY_OP(check_len, ebm::BinaryOp::greater, bool_type, to_set_len, limit);
            EBM_SETTER_STATUS(status, setter_return_type, ebm::SetterStatus::FAILED);
            EBM_RETURN(return_stmt, status, func_ref);
            EBM_IF_STATEMENT(length_check_statement, check_len, return_stmt, {});
            EBM_LENGTH_CHECK(length_check, ebm::LengthCheckType::SETTER_VECTOR_LENGTH, to_set_len, limit, func_ref, length_check_statement);
            // constructs:
            // length_field = (cast)value.length;
            EBM_CAST(casted_len, array_info.length_type, usize_type, to_set_len);
            EBM_ASSIGNMENT(length_assign, array_info.length_field_expr, casted_len);

            // constructs:
            // if (value.length > max_length) {
            //    return error("Array length exceeds maximum");
            // }
            // array_field = value
            // length_field = (cast)value.length;
            ebm::Block new_body;
            append(new_body, length_check);
            append(new_body, assign_ref);
            append(new_body, length_assign);
            EBM_BLOCK(new_block_ref, std::move(new_body));
            return new_block_ref;
        };

        for (size_t i = 0; i < max_index; i++) {
            auto& stmt = tctx.statement_repository().get_all()[i];
            // find assignment and the retrieve parent from branch_info
            if (stmt.body.kind != ebm::StatementKind::ASSIGNMENT) {
                continue;
            }
            auto field_info = branch_info.find(get_id(stmt.id));
            if (field_info == branch_info.end()) {
                continue;
            }
            // field must be in array_length_setters
            auto array_info = array_length_setters.find(get_id(field_info->second.field_ref));
            if (array_info == array_length_setters.end()) {
                continue;
            }
            // get assignments value
            MAYBE(value_ref, stmt.body.value());

            // ^^^ INVALIDATES above references from repository ^^^
            // Note: field_info and array_info are only holds references to statements, and they are not modified directly, so they should still be valid.
            MAYBE(new_block_ref, derive_setter(field_info->second.func_ref, field_info->second.assign_ref, array_info->second, value_ref));

            MAYBE(parent_stmt, tctx.statement_repository().get(field_info->second.parent_ref));
            parent_stmt.body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                if constexpr (std::is_same_v<std::decay_t<decltype(val)>, ebm::StatementRef>) {
                    if (get_id(val) == get_id(field_info->second.assign_ref)) {
                        val.id = new_block_ref.id;
                    }
                }
                else
                    VISITOR_RECURSE_CONTAINER(visitor, name, val)
                else VISITOR_RECURSE(visitor, name, val)
            });

            // remove already handled field
            array_length_setters.erase(get_id(field_info->second.field_ref));
        }

        // next, derive vector setter for remaining fields that are not handled in the above loop (e.g. those that are not used in property setters)
        for (size_t i = 0; i < max_index; i++) {
            auto& stmt = tctx.statement_repository().get_all()[i];
            if (stmt.body.kind != ebm::StatementKind::FIELD_DECL) {
                continue;
            }
            auto array_info = array_length_setters.find(get_id(stmt.id));
            if (array_info == array_length_setters.end()) {
                continue;
            }
            ebm::FunctionDecl func_decl;
            func_decl.kind = ebm::FunctionKind::VECTOR_SETTER;
            MAYBE(field, access_field<"field_decl">(tctx.context().repository(), array_info->second.array_field_ref));
            func_decl.parent_format = field.parent_struct;
            func_decl.return_type = setter_return_type;
            func_decl.name = field.name;
            func_decl.property(to_weak(array_info->second.array_field_ref));
            MAYBE(new_fn_id, ctx.repository().new_statement_id());

            // INVALIDATES above references ^^^^^
            EBM_DEFINE_PARAMETER(param, {}, array_info->second.vector_type, false);
            append(func_decl.params, param_def);
            MAYBE(state_params, ctx.state().get_format_encode_decode(from_weak(func_decl.parent_format)));
            for (auto& param : state_params.state_variables) {
                append(func_decl.params, param.prop_set_var_def);
            }
            // assign parameter to value_ref
            // INVALIDATES above references ^^^^
            // constructs:
            // param = value
            EBM_ASSIGNMENT(param_assign, array_info->second.array_field_expr, param);
            MAYBE(new_block_ref, derive_setter(
                                     new_fn_id,
                                     param_assign,
                                     array_info->second, param));
            EBM_SETTER_STATUS(success, setter_return_type, ebm::SetterStatus::SUCCESS);
            EBM_RETURN(return_stmt, success, new_fn_id);
            ebm::Block func_body;
            append(func_body, new_block_ref);
            append(func_body, return_stmt);
            EBM_BLOCK(func_body_ref, std::move(func_body));
            func_decl.body = func_body_ref;
            ebm::StatementBody new_body;
            new_body.kind = ebm::StatementKind::FUNCTION_DECL;
            new_body.func_decl(std::move(func_decl));
            EBMA_ADD_STATEMENT(new_stmt, new_fn_id, std::move(new_body));
            // INVALIDATES above references ^^^^
            // insert properties to the struct
            MAYBE(struct_decl, access_field<"struct_decl">(tctx.context().repository(), func_decl.parent_format));
            if (auto props = struct_decl.properties()) {
                append(*props, new_stmt);
            }
            else {
                struct_decl.has_properties(true);
                ebm::Block properties_block;
                append(properties_block, new_stmt);
                struct_decl.properties(std::move(properties_block));
            }
        }
        return {};
    }

}  // namespace ebmgen