/*license*/

#include "ebm/extended_binary_module.hpp"
#include "ebmgen/access.hpp"
#include "ebmgen/converter.hpp"
#include "../convert/helper.hpp"

namespace ebmgen {
    expected<void> add_cast_func(TransformContext& tctx) {
        auto& ctx = tctx.context();
        auto& exprs = tctx.expression_repository().get_all();
        size_t max_size = exprs.size();
        for (size_t i = 0; i < max_size; i++) {
            auto& c = exprs[i];
            if (auto cast = c.body.type_cast_desc()) {
                if (cast->cast_kind != ebm::CastType::FUNCTION_CAST) {
                    continue;
                }
                MAYBE(from_type, ctx.repository().get_type(cast->from_type));
                auto id = from_type.body.id();
                if (!id) {
                    return unexpect_error("from_type has no id in add_cast_func: {}", to_string(from_type.body.kind));
                }
                MAYBE(stmt, ctx.repository().get_statement(id->id));
                auto fmt_decl = stmt.body.struct_decl();
                if (!fmt_decl) {
                    return unexpect_error("from_type is not struct in add_cast_func: {}", to_string(from_type.body.kind));
                }
                auto methods = fmt_decl->methods();
                if (!methods) {
                    return unexpect_error("from_type has no methods in add_cast_func: {}", to_string(from_type.body.kind));
                }
                MAYBE(to_type, ctx.repository().get_type(c.body.type));
                bool is_int_like = to_type.body.kind == ebm::TypeKind::INT ||
                                   to_type.body.kind == ebm::TypeKind::UINT ||
                                   to_type.body.kind == ebm::TypeKind::USIZE;
                size_t int_like_size = 0;
                ebm::WeakStatementRef candidate;
                for (auto& m : methods->container) {
                    MAYBE(decl, ctx.repository().get_statement(m));
                    auto func_decl = decl.body.func_decl();
                    if (!func_decl) {
                        return unexpect_error("method is not function in add_cast_func: {}", to_string(decl.body.kind));
                    }
                    if (func_decl->kind != ebm::FunctionKind::CAST) {
                        continue;
                    }
                    if (get_id(func_decl->return_type) == get_id(c.body.type)) {
                        candidate = to_weak(m);
                        break;
                    }
                    if (is_int_like) {
                        MAYBE(ret_type, ctx.repository().get_type(func_decl->return_type));
                        if ((ret_type.body.kind == ebm::TypeKind::INT ||
                             ret_type.body.kind == ebm::TypeKind::UINT ||
                             ret_type.body.kind == ebm::TypeKind::USIZE)) {
                            candidate = to_weak(m);
                        }
                    }
                }
                if (is_nil(candidate)) {
                    return unexpect_error("no suitable cast function found in add_cast_func for type '{}'", to_string(from_type.body.kind));
                }

                MAYBE(func, access_field<"func_decl">(ctx.repository(), candidate));
                ebm::FuncTypeDesc fn_type;
                fn_type.return_type = func.return_type;
                for (auto& arg : func.params.container) {
                    MAYBE(param, access_field<"param_decl">(ctx.repository(), arg));
                    append(fn_type.params, param.param_type);
                }
                fn_type.annotation(ebm::FuncTypeAnnotation::METHOD);
                ebm::TypeBody fn_type_body{.kind = ebm::TypeKind::FUNCTION};
                fn_type_body.func_desc(std::move(fn_type));
                EBMA_ADD_TYPE(fn_type_ref, std::move(fn_type_body));

                // BE CAREFUL, from here we modify exprs
                auto cast_copy = *cast;
                EBM_IDENTIFIER(ident, candidate.id, fn_type_ref);
                EBM_MEMBER_ACCESS(member, fn_type_ref, cast_copy.source_expr, ident);
                ebm::CallDesc call;
                call.callee = member;
                EBM_CALL(method_call, fn_type.return_type, std::move(call));
                exprs[i].body.type_cast_desc()->cast_call(ebm::LoweredExpressionRef{method_call});
            }
        }
        return {};
    }
}  // namespace ebmgen