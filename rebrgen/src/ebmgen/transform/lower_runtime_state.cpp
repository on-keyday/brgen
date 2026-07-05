/*license*/
#include <unordered_map>
#include "ebm/extended_binary_module.hpp"
#include "../converter.hpp"
#include "../visitor/visitor.hpp"
#include "ebmgen/converter.hpp"
#include "ebmgen/mapping.hpp"
#include "to_mapping_table.hpp"
#include "ebmcodegen/stub/make_visitor.hpp"
#include "../convert/helper.hpp"
#include "transform.hpp"

namespace ebmgen {

    // ADR 0039: bundle runtime-only state (stream offset, bit offset) into a
    // compiler-managed companion struct (`RuntimeState`) and thread it only
    // through the encode/decode functions that actually need it.
    //
    // This pass runs after propagate_io_input_desc, so the IOInputDesc flags
    // (has_absolute_offset / has_bit_offset) on coder input types are already
    // fixpoint-propagated across the whole io type graph. What this pass does:
    //   1. gate: collect ENCODE/DECODE functions whose io param type requires offset
    //   2. synthesize the RuntimeState struct (offset, optionally bit_offset)
    //   3. append an is_runtime_state parameter to each gated function
    //   4. create public wrappers for gated functions lacking one (signature keep)
    //   5. patch call sites: pass the companion down (INOUT), wrappers seed a local
    //   6. lower GET_STREAM_OFFSET: lowered_expr = MEMBER_ACCESS(runtime_state, offset|bit_offset)
    //
    // The offset *increment* (`+= size`) is intentionally NOT inserted here; per
    // ADR 0008 it stays in each backend's READ_DATA/WRITE_DATA visitors because
    // the actual stream advance timing depends on the backend's IO strategy.

    namespace {
        struct RuntimeStateGate {
            bool absolute = false;
            bool bit = false;

            bool gated() const {
                return absolute || bit;
            }
        };

        expected<RuntimeStateGate> gate_of_function(TransformContext& tctx, const ebm::FunctionDecl& func) {
            RuntimeStateGate gate;
            for (auto& param_ref : func.params.container) {
                MAYBE(param_stmt, tctx.context().repository().get_statement(param_ref));
                MAYBE(param_decl, param_stmt.body.param_decl());
                MAYBE(type, tctx.context().repository().get_type(param_decl.param_type));
                if (auto* desc = type.body.io_input_desc()) {
                    gate.absolute = gate.absolute || desc->has_absolute_offset();
                    gate.bit = gate.bit || desc->has_bit_offset();
                }
            }
            return gate;
        }
    }  // namespace

    expected<void> lower_runtime_state(TransformContext& tctx) {
        auto& ctx = tctx.context();

        // --- 1. collect gated encode/decode functions (wrappers excluded) ---
        std::vector<ebm::StatementRef> targets;
        RuntimeStateGate global_gate;
        {
            auto& all_stmts = tctx.statement_repository().get_all();
            size_t max_index = all_stmts.size();
            for (size_t i = 0; i < max_index; i++) {
                auto* func = all_stmts[i].body.func_decl();
                if (!func) {
                    continue;
                }
                if (func->kind != ebm::FunctionKind::ENCODE && func->kind != ebm::FunctionKind::DECODE) {
                    continue;
                }
                if (func->attribute.is_wrapper()) {
                    continue;
                }
                MAYBE(gate, gate_of_function(tctx, *func));
                if (!gate.gated()) {
                    continue;
                }
                global_gate.absolute = global_gate.absolute || gate.absolute;
                global_gate.bit = global_gate.bit || gate.bit;
                targets.push_back(all_stmts[i].id);
            }
        }
        if (targets.empty()) {
            return {};
        }

        // --- 2. synthesize the RuntimeState struct ---
        EBMU_COUNTER_TYPE(counter_type);
        MAYBE(struct_id, ctx.repository().new_statement_id());
        ebm::StatementRef offset_field_ref;
        ebm::StatementRef bit_offset_field_ref;
        {
            ebm::StructDecl runtime_state_decl;
            EBMA_ADD_IDENTIFIER(struct_name, std::string("RuntimeState"));
            runtime_state_decl.name = struct_name;
            auto make_field = [&](const char* name) -> expected<ebm::StatementRef> {
                EBMA_ADD_IDENTIFIER(field_name, std::string(name));
                ebm::FieldDecl field;
                field.name = field_name;
                field.field_type = counter_type;
                field.parent_struct = to_weak(struct_id);
                ebm::StatementBody field_body;
                field_body.kind = ebm::StatementKind::FIELD_DECL;
                field_body.field_decl(std::move(field));
                EBMA_ADD_STATEMENT(field_ref, std::move(field_body));
                return field_ref;
            };
            // byte offset is the base runtime state; bit_offset joins only when some
            // io in the module actually observes bit positions
            MAYBE(offset_ref, make_field("offset"));
            offset_field_ref = offset_ref;
            append(runtime_state_decl.fields, offset_field_ref);
            if (global_gate.bit) {
                MAYBE(bit_ref, make_field("bit_offset"));
                bit_offset_field_ref = bit_ref;
                append(runtime_state_decl.fields, bit_offset_field_ref);
            }
            ebm::StatementBody struct_body;
            struct_body.kind = ebm::StatementKind::STRUCT_DECL;
            struct_body.struct_decl(std::move(runtime_state_decl));
            EBMA_ADD_STATEMENT(struct_stmt, struct_id, std::move(struct_body));
            // attach to the program so backends emit the struct definition
            bool attached = false;
            for (auto& s : tctx.statement_repository().get_all()) {
                if (s.body.kind == ebm::StatementKind::PROGRAM_DECL) {
                    if (auto* block = s.body.block()) {
                        append(*block, struct_stmt);
                        attached = true;
                        break;
                    }
                }
            }
            if (!attached) {
                return unexpect_error("lower_runtime_state: PROGRAM_DECL not found; cannot place RuntimeState struct");
            }
        }
        ebm::TypeRef runtime_state_type;
        {
            ebm::TypeBody struct_type_body;
            struct_type_body.kind = ebm::TypeKind::STRUCT;
            struct_type_body.id(to_weak(struct_id));
            EBMA_ADD_TYPE(type_ref, std::move(struct_type_body));
            runtime_state_type = type_ref;
        }

        // --- 3. append the companion parameter to each gated function ---
        std::unordered_map<std::uint64_t, ebm::StatementRef> fn_to_param;  // gated fn id -> param stmt
        for (auto& target : targets) {
            EBMA_ADD_IDENTIFIER(param_name, std::string("runtime_state"));
            auto param_body = make_parameter_decl(param_name, runtime_state_type, false, target);
            {
                auto* param_decl = param_body.param_decl();
                if (!param_decl) {
                    return unexpect_error("lower_runtime_state: make_parameter_decl returned non-parameter body");
                }
                param_decl->is_runtime_state(true);
                // backends increment offsets through the companion, so it is INOUT
                param_decl->is_mutated(true);
            }
            EBMA_ADD_STATEMENT(param_ref, std::move(param_body));
            MAYBE(fn_stmt, tctx.statement_repository().get(target));
            MAYBE(func, fn_stmt.body.func_decl());
            append(func.params, param_ref);
            fn_to_param[get_id(target)] = param_ref;
        }

        // --- 4. create wrappers for gated functions that lack one, so public
        //        signatures stay unchanged (mirrors derive_encode_decode_wrapper) ---
        for (auto& target : targets) {
            MAYBE(func_stmt, ctx.repository().get_statement(target));
            MAYBE(func, func_stmt.body.func_decl());
            if (func.attribute.has_wrapper()) {
                continue;  // existing (state-variable) wrapper is patched in step 5
            }
            bool is_encode = func.kind == ebm::FunctionKind::ENCODE;
            MAYBE(encdec, ctx.state().get_format_encode_decode(from_weak(func.parent_format)));

            auto orig_name = func.name;
            auto return_type = func.return_type;
            auto parent_format = func.parent_format;
            auto kind = func.kind;
            auto is_mutable = func.attribute.is_mutable();
            auto impl_expr = is_encode ? encdec.encode : encdec.decode;
            auto impl_type = is_encode ? encdec.encode_type : encdec.decode_type;
            auto stream_input = is_encode ? encdec.encoder_input : encdec.decoder_input;
            auto stream_param = is_encode ? encdec.encoder_input_def : encdec.decoder_input_def;

            MAYBE(orig_ident, tctx.identifier_repository().get(orig_name));
            auto impl_name_str = orig_ident.body.data + "_impl";
            MAYBE(impl_name_id, ctx.repository().add_identifier(impl_name_str));
            ctx.repository().get_statement(target)->body.func_decl()->name = impl_name_id;

            MAYBE(wrapper_id, ctx.repository().new_statement_id());

            ebm::Block wrapper_body_block;

            ebm::TypeBody struct_type_body;
            struct_type_body.kind = ebm::TypeKind::STRUCT;
            struct_type_body.id(parent_format);
            EBMA_ADD_TYPE(struct_type_ref, std::move(struct_type_body));
            EBM_SELF(self_expr, struct_type_ref);
            EBM_MEMBER_ACCESS(callee_expr, impl_type, self_expr, impl_expr);

            // the runtime_state argument itself is appended by the generic
            // call-site patching below (step 5), same as pre-existing wrappers
            ebm::CallDesc call_desc;
            call_desc.callee = callee_expr;
            MAYBE(stream_expr_body, ctx.repository().get_expression(stream_input));
            EBM_AS_ARG(stream_arg, stream_expr_body.body.type, stream_input);
            append(call_desc.arguments, stream_arg);

            EBM_CALL(call_expr, return_type, std::move(call_desc));
            EBM_RETURN(ret_stmt, call_expr, wrapper_id);
            append(wrapper_body_block, ret_stmt);
            EBM_BLOCK(body_ref, std::move(wrapper_body_block));

            ebm::FunctionDecl wrapper_func;
            wrapper_func.name = orig_name;
            wrapper_func.return_type = return_type;
            wrapper_func.kind = kind;
            wrapper_func.parent_format = parent_format;
            wrapper_func.attribute.is_mutable(is_mutable);
            wrapper_func.attribute.is_wrapper(true);
            append(wrapper_func.params, stream_param);
            wrapper_func.body = body_ref;

            ebm::StatementBody wrapper_stmt_body;
            wrapper_stmt_body.kind = ebm::StatementKind::FUNCTION_DECL;
            wrapper_stmt_body.func_decl(std::move(wrapper_func));
            EBMA_ADD_STATEMENT(wrapper_stmt, wrapper_id, std::move(wrapper_stmt_body));

            MAYBE(impl_stmt, tctx.statement_repository().get(target));
            MAYBE(impl_func, impl_stmt.body.func_decl());
            impl_func.attribute.has_wrapper(true);
            impl_func.wrapper_function(wrapper_stmt);
        }

        // --- 5+6. collect patch sites (calls into gated fns, GET_STREAM_OFFSET) ---
        struct CallPatch {
            ebm::ExpressionRef call;
            ebm::StatementRef callee_fn;
            ebm::StatementRef enclosing_fn;
        };
        struct OffsetPatch {
            ebm::ExpressionRef expr;
            ebm::SizeUnit unit;
            ebm::StatementRef enclosing_fn;
        };
        std::vector<CallPatch> call_patches;
        std::vector<OffsetPatch> offset_patches;
        std::unordered_map<std::uint64_t, std::uint64_t> seen_exprs;  // expr id -> enclosing fn id

        visitor::BaseVisitor visitor{to_mapping_table(tctx)};
        visitor.module_.build_maps(mapping::BuildMapOption::NONE);
        visitor::InitialContext ictx{.visitor = visitor};
        ebm::StatementRef current_fn;
        auto note_seen = [&](ebm::ExpressionRef expr) -> expected<bool> {
            auto [it, inserted] = seen_exprs.try_emplace(get_id(expr), get_id(current_fn));
            if (!inserted && it->second != get_id(current_fn)) {
                // expressions live in a central table; the lowering below binds an
                // expression to the enclosing function's companion param, so a
                // shared expression across functions would be ambiguous
                return unexpect_error("lower_runtime_state: expression {} is shared across functions {} and {}", get_id(expr), it->second, get_id(current_fn));
            }
            return inserted;
        };
        auto collector =
            ebmcodegen::util::make_visitor<void>(visitor)
                .name("RuntimeStateCollector")
                .not_context("Type")
                .not_before_or_after()
                .on([&](auto&& self, visitor::Context_Expression_CALL& vctx) -> expected<void> {
                    if (auto callee_ref = vctx.get_field<"member.body.id">(vctx.call_desc.callee)) {
                        auto callee_fn = from_weak(*callee_ref);
                        if (fn_to_param.contains(get_id(callee_fn))) {
                            MAYBE(fresh, note_seen(vctx.item_id));
                            if (fresh) {
                                call_patches.push_back({vctx.item_id, callee_fn, current_fn});
                            }
                        }
                    }
                    return traverse_children<void>(self, vctx);
                })
                .on([&](auto&& self, visitor::Context_Expression_GET_STREAM_OFFSET& vctx) -> expected<void> {
                    MAYBE(fresh, note_seen(vctx.item_id));
                    if (fresh) {
                        offset_patches.push_back({vctx.item_id, vctx.unit, current_fn});
                    }
                    return traverse_children<void>(self, vctx);
                })
                // lowered paths hold LoweredStatementRef, which generic traversal
                // does not follow; visit them explicitly because backends emit them
                .on([&](auto&& self, visitor::Context_Statement_READ_DATA& vctx) -> expected<void> {
                    if (auto* lowered = vctx.read_data.lowered_statement()) {
                        MAYBE_VOID(visited, vctx.visit<void>(self, lowered->io_statement.id));
                    }
                    return traverse_children<void>(self, vctx);
                })
                .on([&](auto&& self, visitor::Context_Statement_WRITE_DATA& vctx) -> expected<void> {
                    if (auto* lowered = vctx.write_data.lowered_statement()) {
                        MAYBE_VOID(visited, vctx.visit<void>(self, lowered->io_statement.id));
                    }
                    return traverse_children<void>(self, vctx);
                })
                .on([&](auto&& self, visitor::Context_Statement_LOWERED_IO_STATEMENTS& vctx) -> expected<void> {
                    for (auto& lowered : vctx.lowered_io_statements.container) {
                        MAYBE_VOID(visited, vctx.visit<void>(self, lowered.io_statement.id));
                    }
                    return traverse_children<void>(self, vctx);
                })
                .on_default_traverse_children()
                .build();
        {
            auto& all_stmts = tctx.statement_repository().get_all();
            size_t max_index = all_stmts.size();
            for (size_t i = 0; i < max_index; i++) {
                auto* func = all_stmts[i].body.func_decl();
                if (!func) {
                    continue;
                }
                current_fn = all_stmts[i].id;
                MAYBE_VOID(visited, ictx.visit(collector, func->body));
            }
        }

        // resolve the companion expression usable inside a function:
        // gated impl -> its own parameter; wrapper -> a zero-value local seeded
        // at block head (offset starts at 0 for a top-level stream)
        std::unordered_map<std::uint64_t, ebm::ExpressionRef> local_companion;  // wrapper fn id -> local var expr
        auto companion_expr_of = [&](ebm::StatementRef fn) -> expected<ebm::ExpressionRef> {
            if (auto it = fn_to_param.find(get_id(fn)); it != fn_to_param.end()) {
                EBM_IDENTIFIER(param_expr, it->second, runtime_state_type);
                return param_expr;
            }
            if (auto it = local_companion.find(get_id(fn)); it != local_companion.end()) {
                return it->second;
            }
            MAYBE(fn_stmt, ctx.repository().get_statement(fn));
            MAYBE(func, fn_stmt.body.func_decl());
            if (!func.attribute.is_wrapper()) {
                return unexpect_error("lower_runtime_state: function {} needs a RuntimeState companion but is neither gated nor a wrapper", get_id(fn));
            }
            auto body_ref = func.body;
            EBMA_ADD_IDENTIFIER(local_name, std::string("runtime_state"));
            EBM_DEFAULT_VALUE(init_expr, runtime_state_type);
            EBM_DEFINE_VARIABLE(local_var, local_name, runtime_state_type, init_expr, ebm::VariableDeclKind::MUTABLE, false);
            MAYBE(body_stmt, tctx.statement_repository().get(body_ref));
            auto* block = body_stmt.body.block();
            if (!block) {
                return unexpect_error("lower_runtime_state: wrapper {} body is not a block", get_id(fn));
            }
            block->container.insert(block->container.begin(), local_var_def);
            MAYBE(new_len, varint(block->container.size()));
            block->len = new_len;
            local_companion[get_id(fn)] = local_var;
            return local_var;
        };

        // --- 5. thread the companion through call sites ---
        for (auto& patch : call_patches) {
            MAYBE(rs_expr, companion_expr_of(patch.enclosing_fn));
            auto callee_param = fn_to_param[get_id(patch.callee_fn)];
            // same convention as state variables (decode.cpp): forwarding the
            // enclosing function's own param is a plain arg (borrow-friendly
            // reborrow in rust etc.), only a wrapper's local companion is INOUT
            bool is_inout = !fn_to_param.contains(get_id(patch.enclosing_fn));
            EBMA_ADD_EXPR(arg_ref, make_as_arg(runtime_state_type, rs_expr, is_inout, callee_param));
            auto* call_expr = ctx.repository().get_expression(patch.call);
            if (!call_expr) {
                return unexpect_error("lower_runtime_state: call expression {} not found", get_id(patch.call));
            }
            auto* call_desc = call_expr->body.call_desc();
            if (!call_desc) {
                return unexpect_error("lower_runtime_state: expression {} is not a call", get_id(patch.call));
            }
            append(call_desc->arguments, arg_ref);
        }

        // --- 6. lower GET_STREAM_OFFSET to a companion member access ---
        for (auto& patch : offset_patches) {
            ebm::StatementRef field_ref;
            if (patch.unit == ebm::SizeUnit::BYTE_FIXED) {
                field_ref = offset_field_ref;
            }
            else if (patch.unit == ebm::SizeUnit::BIT_FIXED) {
                if (is_nil(bit_offset_field_ref)) {
                    return unexpect_error("lower_runtime_state: GET_STREAM_OFFSET({}) wants bit offset but no io in the module was flagged has_bit_offset", get_id(patch.expr));
                }
                field_ref = bit_offset_field_ref;
            }
            else {
                return unexpect_error("lower_runtime_state: GET_STREAM_OFFSET({}) has unsupported unit", get_id(patch.expr));
            }
            MAYBE(rs_expr, companion_expr_of(patch.enclosing_fn));
            EBM_IDENTIFIER(field_expr, field_ref, counter_type);
            EBM_MEMBER_ACCESS(member_access, counter_type, rs_expr, field_expr);
            auto* offset_expr = ctx.repository().get_expression(patch.expr);
            if (!offset_expr) {
                return unexpect_error("lower_runtime_state: GET_STREAM_OFFSET expression {} not found", get_id(patch.expr));
            }
            offset_expr->body.lowered_expr(ebm::LoweredExpressionRef{member_access});
        }

        // --- 7. fail-loud invariant check over the flat expression table:
        //        every call to a gated function must carry the companion argument,
        //        and every GET_STREAM_OFFSET must have been lowered. This catches
        //        containers the structural traversal above failed to reach.
        {
            auto resolve_callee_fn = [&](const ebm::CallDesc& call_desc) -> std::optional<ebm::StatementRef> {
                auto* callee = ctx.repository().get_expression(call_desc.callee);
                if (!callee || callee->body.kind != ebm::ExpressionKind::MEMBER_ACCESS) {
                    return std::nullopt;
                }
                auto member = callee->body.member();
                if (!member) {
                    return std::nullopt;
                }
                auto* member_expr = ctx.repository().get_expression(*member);
                if (!member_expr || member_expr->body.kind != ebm::ExpressionKind::IDENTIFIER) {
                    return std::nullopt;
                }
                auto id = member_expr->body.id();
                if (!id) {
                    return std::nullopt;
                }
                return from_weak(*id);
            };
            for (auto& expr : tctx.expression_repository().get_all()) {
                if (auto* call_desc = expr.body.call_desc()) {
                    auto callee_fn = resolve_callee_fn(*call_desc);
                    if (!callee_fn) {
                        continue;
                    }
                    auto it = fn_to_param.find(get_id(*callee_fn));
                    if (it == fn_to_param.end()) {
                        continue;
                    }
                    bool has_companion = false;
                    for (auto& arg_ref : call_desc->arguments.container) {
                        auto* arg = ctx.repository().get_expression(arg_ref);
                        if (!arg) {
                            continue;
                        }
                        if (auto* as_arg = arg->body.as_arg()) {
                            if (get_id(from_weak(as_arg->param)) == get_id(it->second)) {
                                has_companion = true;
                                break;
                            }
                        }
                    }
                    if (!has_companion) {
                        return unexpect_error("lower_runtime_state: call {} targets gated function {} but was not reached by the traversal (missing runtime_state argument)", get_id(expr.id), get_id(*callee_fn));
                    }
                }
                else if (expr.body.kind == ebm::ExpressionKind::GET_STREAM_OFFSET) {
                    auto lowered = expr.body.lowered_expr();
                    if (!lowered || is_nil(lowered->id)) {
                        return unexpect_error("lower_runtime_state: GET_STREAM_OFFSET {} was not reached by the traversal (lowered_expr not set)", get_id(expr.id));
                    }
                }
            }
        }

        return {};
    }

}  // namespace ebmgen
