/*license*/
#include "transform.hpp"
#include "ebm/extended_binary_module.hpp"
#include "../convert/helper.hpp"

namespace ebmgen {
    expected<void> derive_property_setter_getter(TransformContext& tctx) {
        auto& ctx = tctx.context();
        auto& all_stmts = tctx.statement_repository().get_all();
        size_t current_index = all_stmts.size();
        for (size_t i = 0; i < current_index; i++) {
            auto& s = all_stmts[i];
            if (auto prop = s.body.property_decl()) {
                auto copy = *prop;  // avoid effect of memory relocation of adding object
                prop = &copy;
                ebm::FunctionDecl getter, setter;
                MAYBE(getter_id, ctx.repository().new_statement_id());
                MAYBE(setter_id, ctx.repository().new_statement_id());
                getter.kind = ebm::FunctionKind::PROPERTY_GETTER;
                setter.kind = ebm::FunctionKind::PROPERTY_SETTER;
                setter.property(to_weak(s.id));
                getter.property(to_weak(s.id));
                getter.name = prop->name;
                setter.name = prop->name;
                getter.parent_format = prop->parent_format;
                setter.parent_format = prop->parent_format;
                MAYBE(encdec_ref, ctx.state().get_format_encode_decode(from_weak(getter.parent_format)));
                auto copy_state_vars = encdec_ref.state_variables;
                // getter return value
                {
                    ebm::TypeBody ptr_type;
                    ptr_type.kind = prop->merge_mode == ebm::MergeMode::STRICT_TYPE ? ebm::TypeKind::PTR : ebm::TypeKind::OPTIONAL;
                    if (ptr_type.kind == ebm::TypeKind::PTR) {
                        ptr_type.pointee_type(prop->property_type);
                    }
                    else {
                        ptr_type.inner_type(prop->property_type);
                    }
                    EBMA_ADD_TYPE(ret_type, std::move(ptr_type));
                    getter.return_type = ret_type;

                    for (auto& v : copy_state_vars) {
                        append(getter.params, v.prop_get_var_def);
                    }
                }
                // setter return value and arguments
                EBM_DEFINE_PARAMETER(arg, {}, prop->property_type, false);
                {
                    ebm::TypeBody set_return;
                    set_return.kind = ebm::TypeKind::PROPERTY_SETTER_RETURN;
                    EBMA_ADD_TYPE(ret_type, std::move(set_return));
                    setter.return_type = ret_type;
                    append(setter.params, arg_def);

                    for (auto& v : copy_state_vars) {
                        append(setter.params, v.prop_set_var_def);
                    }
                }
                // getter match
                {
                    ebm::MatchStatement m;
                    m.target = prop->getter_condition;
                    EBM_DEFAULT_VALUE(default_, getter.return_type);  // nullptr
                    EBM_RETURN(default_return, default_, getter_id);
                    for (auto& b : prop->members.container) {
                        MAYBE(stmt, ctx.repository().get_statement(b));
                        MAYBE(member_ref, stmt.body.property_member_decl());
                        auto member = member_ref;  // copy to avoid memory relocation
                        ebm::MatchBranch br;
                        br.condition = make_condition(member.getter_condition);
                        if (is_nil(member.field)) {
                            br.body = default_return;
                        }
                        else {
                            MAYBE(self_expr, ctx.state().get_self_ref_for_id(member.field));
                            ebm::StatementRef ret;
                            if (prop->merge_mode == ebm::MergeMode::STRICT_TYPE) {
                                EBM_ADDRESSOF(addr, getter.return_type, self_expr);
                                EBM_RETURN(ret_, addr, getter_id);
                                ret = ret_;
                            }
                            else {
                                MAYBE(self_expr_body, ctx.repository().get_expression(self_expr));
                                auto expr_type = self_expr_body.body.type;
                                EBM_CAST(casted, prop->property_type, expr_type, self_expr);
                                EBM_OPTIONALOF(opt, getter.return_type, casted);
                                EBM_RETURN(ret_, opt, getter_id);
                                ret = ret_;
                            }
                            MAYBE(field_stmt, ctx.repository().get_statement(member.field));
                            if (auto field_decl = field_stmt.body.field_decl()) {
                                ebm::TypeBody body{.kind = ebm::TypeKind::STRUCT};
                                body.id(field_decl->parent_struct);
                                EBMA_ADD_TYPE(struct_type_ref, std::move(body));
                                MAYBE(handle_union, handle_variant_alternative(ctx, struct_type_ref, ebm::InitCheckType::union_get, getter_id));
                                ebm::Block block;
                                append(block, handle_union->first);
                                append(block, ret);
                                EBM_BLOCK(block_ref, std::move(block));
                                br.body = block_ref;
                            }
                            else {
                                br.body = ret;
                            }
                        }
                        ebm::StatementBody body{.kind = ebm::StatementKind::MATCH_BRANCH};
                        body.match_branch(std::move(br));
                        EBMA_ADD_STATEMENT(s, std::move(body));
                        append(m.branches, s);
                    }
                    MAYBE_VOID(getter_lowered, ctx.get_statement_converter().derive_match_lowered_if(m, false));
                    ebm::StatementBody body{.kind = ebm::StatementKind::MATCH_STATEMENT};
                    body.match_statement(std::move(m));
                    EBMA_ADD_STATEMENT(match, std::move(body));
                    ebm::Block getter_block;
                    append(getter_block, match);
                    append(getter_block, default_return);
                    EBM_BLOCK(gblock, std::move(getter_block));
                    getter.body = gblock;
                }
                // setter match
                {
                    ebm::MatchStatement m;
                    m.target = prop->setter_condition;
                    EBM_SETTER_STATUS(default_status, setter.return_type, ebm::SetterStatus::FAILED);
                    EBM_RETURN(default_return, default_status, setter_id);
                    for (auto& b : prop->members.container) {
                        MAYBE(stmt, ctx.repository().get_statement(b));
                        MAYBE(member_ref, stmt.body.property_member_decl());
                        auto member = member_ref;  // copy to avoid memory relocation
                        ebm::MatchBranch br;
                        br.condition = make_condition(member.setter_condition);
                        if (is_nil(member.field)) {
                            br.body = default_return;
                        }
                        else {
                            ebm::Block block;
                            MAYBE(field_stmt, ctx.repository().get_statement(member.field));
                            if (auto field_decl = field_stmt.body.field_decl()) {
                                ebm::TypeBody body{.kind = ebm::TypeKind::STRUCT};
                                body.id(field_decl->parent_struct);
                                EBMA_ADD_TYPE(struct_type_ref, std::move(body));
                                MAYBE(handle_union, handle_variant_alternative(ctx, struct_type_ref, ebm::InitCheckType::union_set, setter_id));
                                append(block, handle_union->first);
                            }
                            MAYBE(self_expr, ctx.state().get_self_ref_for_id(member.field));
                            MAYBE(field_expr, ctx.repository().get_expression(self_expr));
                            auto expr_type = field_expr.body.type;
                            EBM_CAST(casted_arg, expr_type, prop->property_type, arg);
                            EBM_ASSIGNMENT(assign, self_expr, casted_arg);
                            append(block, assign);
                            EBM_SETTER_STATUS(success_status, setter.return_type, ebm::SetterStatus::SUCCESS);
                            EBM_RETURN(success_return, success_status, setter_id);
                            append(block, success_return);
                            EBM_BLOCK(b, std::move(block));
                            br.body = b;
                        }
                        ebm::StatementBody body{.kind = ebm::StatementKind::MATCH_BRANCH};
                        body.match_branch(std::move(br));
                        EBMA_ADD_STATEMENT(s, std::move(body));
                        append(m.branches, s);
                    }
                    MAYBE_VOID(setter_lowered, ctx.get_statement_converter().derive_match_lowered_if(m, false));
                    ebm::StatementBody body{.kind = ebm::StatementKind::MATCH_STATEMENT};
                    body.match_statement(std::move(m));
                    EBMA_ADD_STATEMENT(match, std::move(body));
                    ebm::Block setter_block;
                    append(setter_block, match);
                    append(setter_block, default_return);
                    EBM_BLOCK(sblock, std::move(setter_block));
                    setter.body = sblock;
                }
                ebm::StatementBody func{.kind = ebm::StatementKind::FUNCTION_DECL};
                func.func_decl(std::move(getter));
                EBMA_ADD_STATEMENT(getter_ref, getter_id, std::move(func));
                func.func_decl(std::move(setter));
                EBMA_ADD_STATEMENT(setter_ref, setter_id, std::move(func));
                prop = all_stmts[i].body.property_decl();  // refresh
                prop->getter_function = ebm::LoweredStatementRef{getter_ref};
                prop->setter_function = ebm::LoweredStatementRef{setter_ref};
            }
        }
        return {};
    }
}  // namespace ebmgen