/*license*/
#include "transform.hpp"
#include "ebm/extended_binary_module.hpp"
#include "../access.hpp"
#include "../converter.hpp"
#include "../convert/helper.hpp"

namespace ebmgen {

    expected<void> derive_encode_decode_wrapper(TransformContext& tctx) {
        auto& ctx = tctx.context();
        auto& all_stmts = tctx.statement_repository().get_all();
        size_t max_index = all_stmts.size();

        // First pass: collect functions that need wrappers
        struct WrapperTarget {
            ebm::StatementRef stmt_id;
            ebm::FunctionKind kind;
            ebm::WeakStatementRef parent_format;
        };
        std::vector<WrapperTarget> targets;

        for (size_t i = 0; i < max_index; i++) {
            auto& s = all_stmts[i];
            auto func = s.body.func_decl();
            if (!func) continue;
            if (func->kind != ebm::FunctionKind::ENCODE && func->kind != ebm::FunctionKind::DECODE) continue;
            if (func->attribute.has_wrapper()) continue;

            bool has_state_vars = false;
            for (auto& param_ref : func->params.container) {
                MAYBE(param_stmt, ctx.repository().get_statement(param_ref));
                MAYBE(param_decl, param_stmt.body.param_decl());
                if (param_decl.is_state_variable()) {
                    has_state_vars = true;
                    break;
                }
            }
            if (!has_state_vars) continue;
            targets.push_back({s.id, func->kind, func->parent_format});
        }

        // Second pass: create wrapper for each target
        for (auto& target : targets) {
            MAYBE(func_stmt, ctx.repository().get_statement(target.stmt_id));
            MAYBE(func, func_stmt.body.func_decl());
            bool is_encode = target.kind == ebm::FunctionKind::ENCODE;
            MAYBE(encdec, ctx.state().get_format_encode_decode(from_weak(target.parent_format)));

            // Copy needed data before potential reallocation
            auto orig_name = func.name;
            auto return_type = func.return_type;
            auto parent_format = func.parent_format;
            auto state_vars_copy = encdec.state_variables;
            auto impl_expr = is_encode ? encdec.encode : encdec.decode;
            auto impl_type = is_encode ? encdec.encode_type : encdec.decode_type;
            auto stream_input = is_encode ? encdec.encoder_input : encdec.decoder_input;
            auto stream_param = is_encode ? encdec.encoder_input_def : encdec.decoder_input_def;

            // Create impl name: "encode" -> "encode_impl"
            MAYBE(orig_ident, tctx.identifier_repository().get(orig_name));
            auto impl_name_str = orig_ident.body.data + "_impl";
            MAYBE(impl_name_id, ctx.repository().add_identifier(impl_name_str));

            // Rename impl function
            ctx.repository().get_statement(target.stmt_id)->body.func_decl()->name = impl_name_id;

            // Create wrapper ID
            MAYBE(wrapper_id, ctx.repository().new_statement_id());

            // Build wrapper body
            ebm::Block wrapper_body_block;
            std::vector<std::pair<ebm::ExpressionRef, ebm::TypeRef>> local_var_exprs;

            for (auto& sv : state_vars_copy) {
                auto var_def = is_encode ? sv.enc_var_def : sv.dec_var_def;
                MAYBE(param_stmt_ptr, ctx.repository().get_statement(var_def));
                MAYBE(param_decl, param_stmt_ptr.body.param_decl());
                auto var_type = param_decl.param_type;
                auto param_name = param_decl.name;

                EBM_DEFAULT_VALUE(init_expr, var_type);
                EBM_DEFINE_VARIABLE(local_var, param_name, var_type, init_expr, ebm::VariableDeclKind::MUTABLE, false);
                append(wrapper_body_block, local_var_def);
                local_var_exprs.push_back({local_var, var_type});
            }

            // SELF + MEMBER_ACCESS to impl
            ebm::TypeBody struct_type_body;
            struct_type_body.kind = ebm::TypeKind::STRUCT;
            struct_type_body.id(parent_format);
            EBMA_ADD_TYPE(struct_type_ref, std::move(struct_type_body));
            EBM_SELF(self_expr, struct_type_ref);
            EBM_MEMBER_ACCESS(callee_expr, impl_type, self_expr, impl_expr);

            // Build CALL
            ebm::CallDesc call_desc;
            call_desc.callee = callee_expr;

            MAYBE(stream_expr_body, ctx.repository().get_expression(stream_input));
            EBM_AS_ARG(stream_arg, stream_expr_body.body.type, stream_input);
            append(call_desc.arguments, stream_arg);

            for (auto& [local_expr, var_type] : local_var_exprs) {
                EBM_AS_INOUT_ARG(state_arg, var_type, local_expr);
                append(call_desc.arguments, state_arg);
            }

            EBM_CALL(call_expr, return_type, std::move(call_desc));
            EBM_RETURN(ret_stmt, call_expr, wrapper_id);
            append(wrapper_body_block, ret_stmt);
            EBM_BLOCK(body_ref, std::move(wrapper_body_block));

            // Create wrapper FunctionDecl
            ebm::FunctionDecl wrapper_func;
            wrapper_func.name = orig_name;
            wrapper_func.return_type = return_type;
            wrapper_func.kind = target.kind;
            wrapper_func.parent_format = parent_format;
            append(wrapper_func.params, stream_param);
            wrapper_func.body = body_ref;

            ebm::StatementBody wrapper_stmt_body;
            wrapper_stmt_body.kind = ebm::StatementKind::FUNCTION_DECL;
            wrapper_stmt_body.func_decl(std::move(wrapper_func));
            EBMA_ADD_STATEMENT(wrapper_stmt, wrapper_id, std::move(wrapper_stmt_body));

            // Update impl: set has_wrapper and wrapper_function
            MAYBE(impl_stmt_ptr, tctx.statement_repository().get(target.stmt_id));
            MAYBE(impl_func_ptr, impl_stmt_ptr.body.func_decl());
            impl_func_ptr.attribute.has_wrapper(true);
            impl_func_ptr.wrapper_function(wrapper_stmt);
        }

        return {};
    }

}  // namespace ebmgen
