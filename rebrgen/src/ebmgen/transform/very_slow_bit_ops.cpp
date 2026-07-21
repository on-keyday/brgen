/*license*/
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <wrap/cout.h>
#include "../converter.hpp"
#include "../convert/helper.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "transform.hpp"

// very_slow_bit_ops (ADR 0046): derive bit-stream variants of ENCODE/DECODE functions.
//
// Each non-wrapper ENCODE/DECODE function gets a cloned twin named
// `<name>_bit_stream` with two extra inout parameters:
//   bit_offset   :u8  (0-7; bit position inside current_byte)
//   current_byte :u8  (partially consumed/filled byte)
// The clone reuses the existing lowered chain structure; only *terminal* IO
// statements (READ_DATA/WRITE_DATA without lowered_statement) are replaced by
// bit-by-bit loops. Terminals are consumption-exact (bit_fields merges only
// finalize byte-aligned routes), so consumption boundaries are preserved.
//
// Reference semantics of one bit read (decode):
//   for i = 0; i < bit_size; i++:
//     if bit_offset == 0:
//       current_byte = read_byte()
//     shift = (7 - bit_offset) if stream_msb_first else bit_offset
//     bit = (current_byte >> shift) & 1
//     target_shift = (bit_size - 1 - i) if mapping_msb_first else i
//     target |= bit << target_shift
//     bit_offset = (bit_offset + 1) & 7
//
// v1 fixes the axes to the diagonal quadrants: endian big/unspec/native ->
// msb-first stream/mapping, little -> lsb-first (matches current byte-IO
// conflation). Terminals with is_peek/has_offset/dynamic endian keep their
// byte-IO form (correct while bit_offset == 0, i.e. at byte-aligned points);
// a warning summary is printed.

namespace ebmgen {

    namespace {

        bool is_decl_boundary(ebm::StatementKind k) {
            switch (k) {
                case ebm::StatementKind::FUNCTION_DECL:
                case ebm::StatementKind::FIELD_DECL:
                case ebm::StatementKind::COMPOSITE_FIELD_DECL:
                case ebm::StatementKind::ENUM_DECL:
                case ebm::StatementKind::ENUM_MEMBER_DECL:
                case ebm::StatementKind::STRUCT_DECL:
                case ebm::StatementKind::UNION_DECL:
                case ebm::StatementKind::UNION_MEMBER_DECL:
                case ebm::StatementKind::PROGRAM_DECL:
                case ebm::StatementKind::PROPERTY_DECL:
                case ebm::StatementKind::PROPERTY_MEMBER_DECL:
                case ebm::StatementKind::METADATA:
                case ebm::StatementKind::IMPORT_MODULE:
                case ebm::StatementKind::ENDIAN_VARIABLE:  // shared endian state; may live outside the function body
                    return true;
                default:
                    return false;
            }
        }

        struct VerySlowDeriver {
            TransformContext& tctx;
            ConverterContext& ctx;

            std::unordered_map<std::uint64_t, ebm::StatementRef> stmt_map;
            std::unordered_map<std::uint64_t, ebm::ExpressionRef> expr_map;
            std::unordered_set<std::uint64_t> variant_funcs;  // new ids of derived variant functions
            std::vector<ebm::StatementRef> cloned_stmts;
            std::vector<ebm::ExpressionRef> cloned_exprs;

            // per-function context (set by derive_one)
            ebm::ExpressionRef bit_offset_expr;
            ebm::ExpressionRef current_byte_expr;

            size_t fallback_count = 0;  // terminals kept in byte-IO form

            expected<ebm::StatementRef> clone_stmt(ebm::StatementRef ref);
            expected<ebm::ExpressionRef> clone_expr(ebm::ExpressionRef ref);

            template <class Body>
            expected<void> rewrite_refs(Body& body) {
                std::optional<Error> deferred;
                body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                    if (deferred) {
                        return;
                    }
                    using U = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<U, ebm::WeakStatementRef>) {
                        // deferred to fix_weak_refs (identity if target not cloned)
                    }
                    else if constexpr (std::is_same_v<U, ebm::LoweredStatementRef>) {
                        auto r = clone_stmt(val.id);
                        if (!r) {
                            deferred = std::move(r.error());
                            return;
                        }
                        val.id = *r;
                    }
                    else if constexpr (std::is_same_v<U, ebm::LoweredExpressionRef>) {
                        auto r = clone_expr(val.id);
                        if (!r) {
                            deferred = std::move(r.error());
                            return;
                        }
                        val.id = *r;
                    }
                    else if constexpr (std::is_same_v<U, ebm::StatementRef>) {
                        auto r = clone_stmt(val);
                        if (!r) {
                            deferred = std::move(r.error());
                            return;
                        }
                        val = *r;
                    }
                    else if constexpr (std::is_same_v<U, ebm::ExpressionRef>) {
                        auto r = clone_expr(val);
                        if (!r) {
                            deferred = std::move(r.error());
                            return;
                        }
                        val = *r;
                    }
                    else if constexpr (std::is_same_v<U, ebm::TypeRef> || std::is_same_v<U, ebm::IdentifierRef> ||
                                       std::is_same_v<U, ebm::StringRef> || std::is_same_v<U, ebm::AnyRef>) {
                        // shared tables; keep as-is
                    }
                    else
                        VISITOR_RECURSE_CONTAINER(visitor, name, val)
                    else VISITOR_RECURSE(visitor, name, val)
                });
                if (deferred) {
                    return unexpect_error(std::move(*deferred));
                }
                return {};
            }

            template <class Body>
            void rewrite_weak(Body& body) {
                body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                    using U = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<U, ebm::WeakStatementRef>) {
                        if (auto it = stmt_map.find(get_id(val.id)); it != stmt_map.end()) {
                            val.id = it->second;
                        }
                    }
                    else if constexpr (std::is_same_v<U, ebm::StatementRef> || std::is_same_v<U, ebm::ExpressionRef> ||
                                       std::is_same_v<U, ebm::LoweredStatementRef> || std::is_same_v<U, ebm::LoweredExpressionRef> ||
                                       std::is_same_v<U, ebm::TypeRef> || std::is_same_v<U, ebm::IdentifierRef> ||
                                       std::is_same_v<U, ebm::StringRef> || std::is_same_v<U, ebm::AnyRef>) {
                        // already final
                    }
                    else
                        VISITOR_RECURSE_CONTAINER(visitor, name, val)
                    else VISITOR_RECURSE(visitor, name, val)
                });
            }

            expected<void> fix_weak_refs() {
                for (auto& ref : cloned_stmts) {
                    auto* s = ctx.repository().get_statement(ref);
                    if (!s) {
                        return unexpect_error("very_slow_bit_ops: cloned statement {} not found", get_id(ref));
                    }
                    rewrite_weak(s->body);
                }
                for (auto& ref : cloned_exprs) {
                    auto* e = ctx.repository().get_expression(ref);
                    if (!e) {
                        return unexpect_error("very_slow_bit_ops: cloned expression {} not found", get_id(ref));
                    }
                    rewrite_weak(e->body);
                }
                // RESERVE_DATA pairs with a terminal WRITE_DATA via weak ref. When that
                // write was replaced by a bit loop, the reservation (byte-IO space
                // pre-check) no longer has a target; neutralize it to an empty block.
                // The per-byte writes inside the loop carry their own space checks.
                for (auto& ref : cloned_stmts) {
                    auto* s = ctx.repository().get_statement(ref);
                    if (!s || s->body.kind != ebm::StatementKind::RESERVE_DATA) {
                        continue;
                    }
                    auto rd = s->body.reserve_data();
                    if (!rd) {
                        continue;
                    }
                    auto* target = ctx.repository().get_statement(from_weak(rd->write_data));
                    if (!target || !target->body.write_data()) {
                        s->body = make_block(ebm::Block{});
                    }
                }
                return {};
            }

            // resolve the function a (cloned) callee expression finally refers to.
            // callee is expected to be MEMBER_ACCESS(..., IDENTIFIER) or IDENTIFIER.
            std::optional<ebm::StatementRef> resolve_callee_func(ebm::ExpressionRef callee) {
                auto* e = ctx.repository().get_expression(callee);
                if (!e) {
                    return std::nullopt;
                }
                if (e->body.kind == ebm::ExpressionKind::MEMBER_ACCESS) {
                    if (auto member = e->body.member()) {
                        return resolve_callee_func(*member);
                    }
                    return std::nullopt;
                }
                if (e->body.kind == ebm::ExpressionKind::IDENTIFIER) {
                    if (auto id = e->body.id()) {
                        return id->id;
                    }
                }
                return std::nullopt;
            }

            // if the call goes to a function that has a bit-stream variant, retarget is
            // done by weak-ref remapping later; here we only append the bit state args.
            expected<void> maybe_append_bit_args(ebm::CallDesc& desc) {
                auto func = resolve_callee_func(desc.callee);
                if (!func) {
                    return {};
                }
                auto it = stmt_map.find(get_id(*func));
                if (it == stmt_map.end() || !variant_funcs.contains(get_id(it->second))) {
                    return {};
                }
                EBMU_U8(u8_ty);
                EBM_AS_INOUT_ARG(bit_offset_arg, u8_ty, bit_offset_expr);
                append(desc.arguments, bit_offset_arg);
                EBM_AS_INOUT_ARG(current_byte_arg, u8_ty, current_byte_expr);
                append(desc.arguments, current_byte_arg);
                return {};
            }

            // emit `for i in 0..n { ...read one bit into dest... }` into `out`.
            // dest must be an assignable expression of type dest_type, zero-initialized by caller.
            expected<void> emit_bit_read(ebm::Block& out, const ebm::IOData& io, ebm::ExpressionRef dest, ebm::TypeRef dest_type, std::uint64_t n, bool stream_msb, bool mapping_msb) {
                EBMU_U8(u8_ty);
                EBMU_BOOL_TYPE(bool_ty);
                EBMU_INT_LITERAL(zero_lit, 0);
                EBMU_INT_LITERAL(one_lit, 1);
                EBMU_INT_LITERAL(seven_lit, 7);

                EBM_COUNTER_LOOP_START(bit_counter);
                ebm::Block lbody;

                // if bit_offset == 0 { buf = read(1); current_byte = buf[0]; }
                EBM_BINARY_OP(off_is_zero, ebm::BinaryOp::equal, bool_ty, bit_offset_expr, zero_lit);
                {
                    ebm::Block read_blk;
                    EBMU_U8_N_ARRAY(u8_arr1, 1, ebm::ArrayAnnotation::read_temporary);
                    EBM_DEFAULT_VALUE(arr_init, u8_arr1);
                    EBM_DEFINE_ANONYMOUS_VARIABLE(read_buf, u8_arr1, arr_init);
                    append(read_blk, read_buf_def);
                    MAYBE(one_byte, make_fixed_size(1, ebm::SizeUnit::BYTE_FIXED));
                    auto iod = make_io_data(io.io_ref, from_weak(io.field), read_buf, u8_arr1, ebm::IOAttribute{}, one_byte);
                    EBM_READ_DATA(read_one, std::move(iod));
                    append(read_blk, read_one);
                    EBM_INDEX(first_byte, u8_ty, read_buf, zero_lit);
                    EBM_ASSIGNMENT(set_cur, current_byte_expr, first_byte);
                    append(read_blk, set_cur);
                    EBM_BLOCK(read_blk_ref, std::move(read_blk));
                    EBM_IF_STATEMENT(if_read, off_is_zero, read_blk_ref, ebm::StatementRef{});
                    append(lbody, if_read);
                }

                // bit = (current_byte >> shift) & 1
                ebm::ExpressionRef stream_shift;
                if (stream_msb) {
                    EBM_BINARY_OP(msb_shift, ebm::BinaryOp::sub, u8_ty, seven_lit, bit_offset_expr);
                    stream_shift = msb_shift;
                }
                else {
                    stream_shift = bit_offset_expr;
                }
                EBM_BINARY_OP(cur_shifted, ebm::BinaryOp::right_shift, u8_ty, current_byte_expr, stream_shift);
                EBM_BINARY_OP(bit_val, ebm::BinaryOp::bit_and, u8_ty, cur_shifted, one_lit);

                // dest |= dest_type(bit) << target_shift
                ebm::ExpressionRef target_shift;
                if (mapping_msb) {
                    EBMU_INT_LITERAL(n_minus_1, n - 1);
                    EBM_BINARY_OP(msb_target_shift, ebm::BinaryOp::sub, dest_type, n_minus_1, bit_counter);
                    target_shift = msb_target_shift;
                }
                else {
                    target_shift = bit_counter;
                }
                EBM_CAST(bit_wide, dest_type, u8_ty, bit_val);
                EBM_BINARY_OP(bit_positioned, ebm::BinaryOp::left_shift, dest_type, bit_wide, target_shift);
                EBM_BINARY_OP(merged, ebm::BinaryOp::bit_or, dest_type, dest, bit_positioned);
                EBM_ASSIGNMENT(store, dest, merged);
                append(lbody, store);

                // bit_offset = (bit_offset + 1) & 7
                EBM_BINARY_OP(off_inc, ebm::BinaryOp::add, u8_ty, bit_offset_expr, one_lit);
                EBM_BINARY_OP(off_wrapped, ebm::BinaryOp::bit_and, u8_ty, off_inc, seven_lit);
                EBM_ASSIGNMENT(set_off, bit_offset_expr, off_wrapped);
                append(lbody, set_off);

                EBM_BLOCK(lbody_ref, std::move(lbody));
                EBMU_INT_LITERAL(n_lit, n);
                EBM_COUNTER_LOOP_END(bit_loop, bit_counter, n_lit, lbody_ref);
                append(out, bit_loop);
                return {};
            }

            // emit `for i in 0..n { ...write one bit of src... }` into `out`.
            // src must be a readable expression of type src_type.
            expected<void> emit_bit_write(ebm::Block& out, const ebm::IOData& io, ebm::ExpressionRef src, ebm::TypeRef src_type, std::uint64_t n, bool stream_msb, bool mapping_msb) {
                EBMU_U8(u8_ty);
                EBMU_BOOL_TYPE(bool_ty);
                EBMU_INT_LITERAL(zero_lit, 0);
                EBMU_INT_LITERAL(one_lit, 1);
                EBMU_INT_LITERAL(seven_lit, 7);
                EBMU_INT_LITERAL(eight_lit, 8);

                EBM_COUNTER_LOOP_START(bit_counter);
                ebm::Block lbody;

                // bit_u8 = u8((src >> target_shift) & 1)
                ebm::ExpressionRef target_shift;
                if (mapping_msb) {
                    EBMU_INT_LITERAL(n_minus_1, n - 1);
                    EBM_BINARY_OP(msb_target_shift, ebm::BinaryOp::sub, src_type, n_minus_1, bit_counter);
                    target_shift = msb_target_shift;
                }
                else {
                    target_shift = bit_counter;
                }
                EBM_BINARY_OP(src_shifted, ebm::BinaryOp::right_shift, src_type, src, target_shift);
                EBM_BINARY_OP(bit_val, ebm::BinaryOp::bit_and, src_type, src_shifted, one_lit);
                EBM_CAST(bit_u8, u8_ty, src_type, bit_val);

                // current_byte |= bit_u8 << shift
                ebm::ExpressionRef stream_shift;
                if (stream_msb) {
                    EBM_BINARY_OP(msb_shift, ebm::BinaryOp::sub, u8_ty, seven_lit, bit_offset_expr);
                    stream_shift = msb_shift;
                }
                else {
                    stream_shift = bit_offset_expr;
                }
                EBM_BINARY_OP(bit_positioned, ebm::BinaryOp::left_shift, u8_ty, bit_u8, stream_shift);
                EBM_BINARY_OP(merged, ebm::BinaryOp::bit_or, u8_ty, current_byte_expr, bit_positioned);
                EBM_ASSIGNMENT(store, current_byte_expr, merged);
                append(lbody, store);

                // bit_offset += 1; if bit_offset == 8 { flush current_byte; reset }
                EBM_BINARY_OP(off_inc, ebm::BinaryOp::add, u8_ty, bit_offset_expr, one_lit);
                EBM_ASSIGNMENT(set_off, bit_offset_expr, off_inc);
                append(lbody, set_off);

                EBM_BINARY_OP(off_is_eight, ebm::BinaryOp::equal, bool_ty, bit_offset_expr, eight_lit);
                {
                    ebm::Block flush_blk;
                    EBMU_U8_N_ARRAY(u8_arr1, 1, ebm::ArrayAnnotation::write_temporary);
                    EBM_DEFAULT_VALUE(arr_init, u8_arr1);
                    EBM_DEFINE_ANONYMOUS_VARIABLE(write_buf, u8_arr1, arr_init);
                    append(flush_blk, write_buf_def);
                    EBM_INDEX(first_byte, u8_ty, write_buf, zero_lit);
                    EBM_ASSIGNMENT(set_byte, first_byte, current_byte_expr);
                    append(flush_blk, set_byte);
                    MAYBE(one_byte, make_fixed_size(1, ebm::SizeUnit::BYTE_FIXED));
                    auto iod = make_io_data(io.io_ref, from_weak(io.field), write_buf, u8_arr1, ebm::IOAttribute{}, one_byte);
                    EBM_WRITE_DATA(write_one, std::move(iod));
                    append(flush_blk, write_one);
                    EBM_ASSIGNMENT(reset_cur, current_byte_expr, zero_lit);
                    append(flush_blk, reset_cur);
                    EBM_ASSIGNMENT(reset_off, bit_offset_expr, zero_lit);
                    append(flush_blk, reset_off);
                    EBM_BLOCK(flush_blk_ref, std::move(flush_blk));
                    EBM_IF_STATEMENT(if_flush, off_is_eight, flush_blk_ref, ebm::StatementRef{});
                    append(lbody, if_flush);
                }

                EBM_BLOCK(lbody_ref, std::move(lbody));
                EBMU_INT_LITERAL(n_lit, n);
                EBM_COUNTER_LOOP_END(bit_loop, bit_counter, n_lit, lbody_ref);
                append(out, bit_loop);
                return {};
            }

            // build the replacement body for a terminal IO statement.
            // returns nullopt when the terminal must keep its byte-IO form (fallback).
            expected<std::optional<ebm::StatementBody>> build_terminal(const ebm::IOData& io, bool write) {
                if (io.attribute.is_peek() || io.attribute.has_offset()) {
                    print_if_verbose("very_slow_bit_ops: keeping byte-IO terminal (peek/offset) for statement io_ref ", get_id(io.io_ref), "\n");
                    fallback_count++;
                    return std::nullopt;
                }
                auto endian = io.attribute.endian();
                if (endian == ebm::Endian::dynamic) {
                    print_if_verbose("very_slow_bit_ops: keeping byte-IO terminal (dynamic endian) for statement io_ref ", get_id(io.io_ref), "\n");
                    fallback_count++;
                    return std::nullopt;
                }
                bool msb = endian != ebm::Endian::little;

                auto* typ = ctx.repository().get_type(io.data_type);
                if (!typ) {
                    return unexpect_error("very_slow_bit_ops: data_type {} not found", get_id(io.data_type));
                }
                auto type_kind = typ->body.kind;

                if (type_kind == ebm::TypeKind::UINT || type_kind == ebm::TypeKind::INT) {
                    std::uint64_t n = 0;
                    if (io.size.unit == ebm::SizeUnit::BIT_FIXED) {
                        n = io.size.size()->value();
                    }
                    else if (io.size.unit == ebm::SizeUnit::BYTE_FIXED) {
                        n = io.size.size()->value() * 8;
                    }
                    else {
                        print_if_verbose("very_slow_bit_ops: keeping byte-IO terminal (non-fixed int size ", to_string(io.size.unit), ")\n");
                        fallback_count++;
                        return std::nullopt;
                    }
                    if (n == 0 || n > 64) {
                        print_if_verbose("very_slow_bit_ops: keeping byte-IO terminal (unsupported bit width ", n, ")\n");
                        fallback_count++;
                        return std::nullopt;
                    }
                    EBMU_UINT_TYPE(un_ty, n);
                    ebm::Block block;
                    if (!write) {
                        EBM_DEFAULT_VALUE(zero_init, un_ty);
                        EBM_DEFINE_ANONYMOUS_VARIABLE(read_tmp, un_ty, zero_init);
                        append(block, read_tmp_def);
                        MAYBE_VOID(loop_ok, emit_bit_read(block, io, read_tmp, un_ty, n, msb, msb));
                        EBM_CAST(final_val, io.data_type, un_ty, read_tmp);
                        EBM_ASSIGNMENT(store_target, io.target, final_val);
                        append(block, store_target);
                    }
                    else {
                        EBM_CAST(src_val, un_ty, io.data_type, io.target);
                        MAYBE_VOID(loop_ok, emit_bit_write(block, io, src_val, un_ty, n, msb, msb));
                    }
                    return make_block(std::move(block));
                }

                if (type_kind == ebm::TypeKind::ARRAY || type_kind == ebm::TypeKind::VECTOR) {
                    auto elem_ref = *typ->body.element_type();
                    auto* elem = ctx.repository().get_type(elem_ref);
                    if (!elem || elem->body.kind != ebm::TypeKind::UINT || elem->body.size()->value() != 8) {
                        print_if_verbose("very_slow_bit_ops: keeping byte-IO terminal (non-u8 element array)\n");
                        fallback_count++;
                        return std::nullopt;
                    }
                    // length expression
                    ebm::ExpressionRef len_expr;
                    if (io.size.unit == ebm::SizeUnit::BYTE_FIXED) {
                        EBMU_INT_LITERAL(len_lit, io.size.size()->value());
                        len_expr = len_lit;
                    }
                    else if (io.size.ref()) {
                        len_expr = *io.size.ref();
                    }
                    else if (write) {
                        EBM_ARRAY_SIZE(dyn_len, io.target);
                        len_expr = dyn_len;
                    }
                    else {
                        print_if_verbose("very_slow_bit_ops: keeping byte-IO terminal (byte array without length, unit ", to_string(io.size.unit), ")\n");
                        fallback_count++;
                        return std::nullopt;
                    }
                    EBMU_U8(u8_ty);
                    ebm::Block block;
                    if (!write) {
                        EBM_DEFAULT_VALUE(arr_init, io.data_type);
                        EBM_DEFINE_ANONYMOUS_VARIABLE(read_arr, io.data_type, arr_init);
                        append(block, read_arr_def);
                        EBM_COUNTER_LOOP_START(byte_counter);
                        ebm::Block obody;
                        EBM_DEFAULT_VALUE(byte_init, u8_ty);
                        EBM_DEFINE_ANONYMOUS_VARIABLE(one_byte_tmp, u8_ty, byte_init);
                        append(obody, one_byte_tmp_def);
                        MAYBE_VOID(loop_ok, emit_bit_read(obody, io, one_byte_tmp, u8_ty, 8, true, true));
                        if (type_kind == ebm::TypeKind::ARRAY) {
                            EBM_INDEX(elem_at, u8_ty, read_arr, byte_counter);
                            EBM_ASSIGNMENT(store_elem, elem_at, one_byte_tmp);
                            append(obody, store_elem);
                        }
                        else {
                            EBM_APPEND(push_elem, read_arr, one_byte_tmp);
                            append(obody, push_elem);
                        }
                        EBM_BLOCK(obody_ref, std::move(obody));
                        EBM_COUNTER_LOOP_END(byte_loop, byte_counter, len_expr, obody_ref);
                        append(block, byte_loop);
                        EBM_ASSIGNMENT(store_target, io.target, read_arr);
                        append(block, store_target);
                    }
                    else {
                        EBM_COUNTER_LOOP_START(byte_counter);
                        ebm::Block obody;
                        EBM_INDEX(elem_at, u8_ty, io.target, byte_counter);
                        MAYBE_VOID(loop_ok, emit_bit_write(obody, io, elem_at, u8_ty, 8, true, true));
                        EBM_BLOCK(obody_ref, std::move(obody));
                        EBM_COUNTER_LOOP_END(byte_loop, byte_counter, len_expr, obody_ref);
                        append(block, byte_loop);
                    }
                    return make_block(std::move(block));
                }

                print_if_verbose("very_slow_bit_ops: keeping byte-IO terminal (unsupported type kind ", to_string(type_kind), ")\n");
                fallback_count++;
                return std::nullopt;
            }

            expected<void> register_method(ebm::WeakStatementRef parent_format, ebm::StatementRef variant) {
                auto format_ref = from_weak(parent_format);
                if (is_nil(format_ref)) {
                    print_if_verbose("very_slow_bit_ops: variant ", get_id(variant), " has no parent format; it may be removed as unused\n");
                    return {};
                }
                auto* fmt = ctx.repository().get_statement(format_ref);
                if (!fmt) {
                    return unexpect_error("very_slow_bit_ops: parent format {} not found", get_id(format_ref));
                }
                auto struct_decl = fmt->body.struct_decl();
                if (!struct_decl) {
                    return unexpect_error("very_slow_bit_ops: parent format {} is not a struct decl but {}", get_id(format_ref), to_string(fmt->body.kind));
                }
                if (struct_decl->has_functions()) {
                    append(*struct_decl->methods(), variant);
                }
                else {
                    struct_decl->has_functions(true);
                    ebm::Block methods;
                    append(methods, variant);
                    struct_decl->methods(std::move(methods));
                }
                return {};
            }

            expected<void> derive_one(ebm::StatementRef orig_ref, ebm::StatementRef variant_ref) {
                auto* orig_stmt = ctx.repository().get_statement(orig_ref);
                if (!orig_stmt) {
                    return unexpect_error("very_slow_bit_ops: function {} not found", get_id(orig_ref));
                }
                auto orig_func = orig_stmt->body.func_decl();
                if (!orig_func) {
                    return unexpect_error("very_slow_bit_ops: statement {} is not a function decl", get_id(orig_ref));
                }
                ebm::FunctionDecl func = *orig_func;  // value copy; pointer may relocate below

                // variant name: <orig>_bit_stream
                MAYBE(orig_ident, tctx.identifier_repository().get(func.name));
                auto variant_name = orig_ident.body.data + "_bit_stream";
                EBMA_ADD_IDENTIFIER(variant_name_ref, variant_name);

                // clone parameters (seeds stmt_map so io_ref/identifier bindings retarget)
                ebm::Block new_params;
                for (auto& p : func.params.container) {
                    MAYBE(cloned_param, clone_stmt(p));
                    append(new_params, cloned_param);
                }

                // bit-stream state parameters
                EBMU_U8(u8_ty);
                EBMA_ADD_IDENTIFIER(bit_offset_name, std::string("bit_offset"));
                EBM_DEFINE_PARAMETER(bit_offset_param, bit_offset_name, u8_ty, false, variant_ref);
                EBMA_ADD_IDENTIFIER(current_byte_name, std::string("current_byte"));
                EBM_DEFINE_PARAMETER(current_byte_param, current_byte_name, u8_ty, false, variant_ref);
                for (auto param_def : {bit_offset_param_def, current_byte_param_def}) {
                    auto* param_stmt = ctx.repository().get_statement(param_def);
                    if (!param_stmt) {
                        return unexpect_error("very_slow_bit_ops: bit state parameter not found");
                    }
                    param_stmt->body.param_decl()->is_mutated(true);
                    append(new_params, param_def);
                }
                bit_offset_expr = bit_offset_param;
                current_byte_expr = current_byte_param;

                // clone body with terminal replacement
                MAYBE(new_body, clone_stmt(func.body));

                func.name = variant_name_ref;
                func.params = std::move(new_params);
                func.body = new_body;
                func.attribute.is_very_slow(true);
                func.attribute.has_wrapper(false);
                func.attribute.is_user_defined(false);

                ebm::StatementBody stmt_body;
                stmt_body.kind = ebm::StatementKind::FUNCTION_DECL;
                auto parent_format = func.parent_format;
                stmt_body.func_decl(std::move(func));
                EBMA_ADD_STATEMENT(added, variant_ref, std::move(stmt_body));
                MAYBE_VOID(reg, register_method(parent_format, added));
                return {};
            }
        };

        expected<ebm::StatementRef> VerySlowDeriver::clone_stmt(ebm::StatementRef ref) {
            if (is_nil(ref)) {
                return ref;
            }
            if (auto it = stmt_map.find(get_id(ref)); it != stmt_map.end()) {
                return it->second;
            }
            auto* stmt = ctx.repository().get_statement(ref);
            if (!stmt) {
                return unexpect_error("very_slow_bit_ops: statement {} not found", get_id(ref));
            }
            if (is_decl_boundary(stmt->body.kind)) {
                return ref;
            }
            MAYBE(new_id, ctx.repository().new_statement_id());
            stmt_map.emplace(get_id(ref), new_id);
            ebm::StatementBody body = stmt->body;  // copy before recursive adds relocate the repository
            MAYBE_VOID(rewrite_ok, rewrite_refs(body));
            // terminal IO replacement: READ_DATA/WRITE_DATA without lowered chain
            if (auto rd = body.read_data(); rd && !rd->lowered_statement()) {
                MAYBE(replaced, build_terminal(*rd, false));
                if (replaced) {
                    body = std::move(*replaced);
                }
            }
            else if (auto wd = body.write_data(); wd && !wd->lowered_statement()) {
                MAYBE(replaced, build_terminal(*wd, true));
                if (replaced) {
                    body = std::move(*replaced);
                }
            }
            EBMA_ADD_STATEMENT(added, new_id, std::move(body));
            cloned_stmts.push_back(added);
            return added;
        }

        expected<ebm::ExpressionRef> VerySlowDeriver::clone_expr(ebm::ExpressionRef ref) {
            if (is_nil(ref)) {
                return ref;
            }
            if (auto it = expr_map.find(get_id(ref)); it != expr_map.end()) {
                return it->second;
            }
            auto* expr = ctx.repository().get_expression(ref);
            if (!expr) {
                return unexpect_error("very_slow_bit_ops: expression {} not found", get_id(ref));
            }
            MAYBE(new_id, ctx.repository().new_expression_id());
            expr_map.emplace(get_id(ref), new_id);
            ebm::ExpressionBody body = expr->body;  // copy before recursive adds relocate the repository
            MAYBE_VOID(rewrite_ok, rewrite_refs(body));
            if (body.kind == ebm::ExpressionKind::CALL) {
                if (auto desc = body.call_desc()) {
                    MAYBE_VOID(args_ok, maybe_append_bit_args(*desc));
                }
            }
            EBMA_ADD_EXPR(added, new_id, std::move(body));
            cloned_exprs.push_back(added);
            return added;
        }

    }  // namespace

    expected<void> derive_very_slow_bit_ops(TransformContext& tctx) {
        auto& ctx = tctx.context();

        struct Target {
            ebm::StatementRef orig;
            ebm::StatementRef variant;
        };
        std::vector<Target> targets;
        {
            auto& all = tctx.statement_repository().get_all();
            for (auto& s : all) {
                auto func = s.body.func_decl();
                if (!func) {
                    continue;
                }
                if (func->kind != ebm::FunctionKind::ENCODE && func->kind != ebm::FunctionKind::DECODE) {
                    continue;
                }
                if (func->attribute.is_wrapper() || func->attribute.is_very_slow()) {
                    continue;
                }
                targets.push_back({s.id, {}});
            }
        }

        VerySlowDeriver deriver{tctx, ctx};
        // pass 1: allocate variant ids and seed the map so that every reference to an
        // original ENCODE/DECODE function (calls, returns, self-recursion) retargets
        // to the bit-stream variant inside cloned bodies.
        for (auto& t : targets) {
            MAYBE(variant_id, ctx.repository().new_statement_id());
            t.variant = variant_id;
            deriver.stmt_map.emplace(get_id(t.orig), variant_id);
            deriver.variant_funcs.insert(get_id(variant_id));
        }
        // pass 2: derive each variant (params + body clone + terminal replacement)
        for (auto& t : targets) {
            MAYBE_VOID(derived, deriver.derive_one(t.orig, t.variant));
        }
        // pass 3: remap weak references (identifier bindings, related_function, loop back-refs)
        MAYBE_VOID(fixed, deriver.fix_weak_refs());

        print_if_verbose("very_slow_bit_ops: derived ", targets.size(), " bit-stream function variants\n");
        if (deriver.fallback_count) {
            auto& cerr = futils::wrap::cerr_wrap();
            cerr << "warning: very_slow_bit_ops: " << deriver.fallback_count
                 << " terminal IO statement(s) kept byte-IO form (peek/offset/dynamic-endian/unsupported); "
                    "they are only correct at byte-aligned bit offsets\n";
        }
        return {};
    }

}  // namespace ebmgen
