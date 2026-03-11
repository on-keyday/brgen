/*license*/
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/convert/helper.hpp"
#include "transform.hpp"
#include <testutil/timer.h>
#include <cstddef>
#include <queue>
#include "../convert/helper.hpp"
#include "bit_manipulator.hpp"
#include <set>
#include <unordered_set>

namespace ebmgen {

    struct Route {
        std::vector<std::shared_ptr<CFG>> route;
        size_t bit_size = 0;
    };

    auto get_io(ebm::Statement& stmt, bool write) {
        return write ? stmt.body.write_data() : stmt.body.read_data();
    }

    std::vector<Route> search_byte_aligned_route(CFGContext& tctx, const std::shared_ptr<CFG>& root, size_t root_size, bool write) {
        std::vector<Route> finalized_routes;
        std::queue<Route> candidates;
        candidates.push({
            .route = {root},
            .bit_size = root_size,
        });  // first route
        while (!candidates.empty()) {
            auto r = candidates.front();
            candidates.pop();
            for (auto& n : r.route.back()->next) {
                auto copy = r;
                copy.route.push_back(n);
                auto stmt = tctx.tctx.statement_repository().get(n->original_node);
                if (!stmt) {
                    continue;  // drop route
                }
                if (auto io_ = get_io(*stmt, write)) {
                    if (io_->size.unit == ebm::SizeUnit::BIT_FIXED || io_->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                        auto add_bit = io_->size.size()->value();
                        if (io_->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                            add_bit *= 8;
                        }
                        copy.bit_size += add_bit;
                        if (copy.bit_size % 8 == 0) {
                            finalized_routes.push_back(std::move(copy));
                            continue;
                        }
                    }
                    else {
                        continue;  // drop route
                    }
                }
                candidates.push(std::move(copy));
            }
        }
        return finalized_routes;
    }

    expected<void> add_lowered_statements(CFGContext& tctx, ebm::IOData& io_copy, ebm::StatementRef node_id, ebm::StatementRef lowered_bit_operation, bool write) {
        auto& ctx = tctx.tctx.context();
        auto low = io_copy.lowered_statement();
        auto bit_shift_to_bit_op = make_lowered_statement(ebm::LoweringIOType::BIT_FIELD_TO_BIT_SHIFT, lowered_bit_operation);
        MAYBE(stmt, tctx.tctx.statement_repository().get_reloc_ptr(node_id));  // refetch because memory is relocated
        if (low) {
            MAYBE(io_, get_io(*stmt, write));
            if (auto low = io_.lowered_statement()) {
                if (low->lowering_type == ebm::LoweringIOType::MULTI_REPRESENTATION) {
                    MAYBE(block, stmt->body.lowered_io_statements());
                    append(block, bit_shift_to_bit_op);
                }
                else {
                    ebm::LoweredIOStatements lows;
                    append(lows, *low);
                    append(lows, bit_shift_to_bit_op);
                    EBM_LOWERED_IO_STATEMENTS(l, std::move(lows));
                    MAYBE(io_, get_io(*stmt, write));  // refetch because memory is relocated
                    io_.attribute.has_lowered_statement(true);
                    io_.lowered_statement(make_lowered_statement(ebm::LoweringIOType::MULTI_REPRESENTATION, l));
                }
            }
            else {
                io_.attribute.has_lowered_statement(true);
                io_.lowered_statement(bit_shift_to_bit_op);
            }
        }
        else {
            MAYBE(io_, get_io(*stmt, write));
            io_.attribute.has_lowered_statement(true);
            io_.lowered_statement(bit_shift_to_bit_op);
        }
        return {};
    }

    // multiple routes merged finally
    // like this:
    //     - b -
    // a -<      >- d
    //     - c -
    expected<void> multi_route_merged(CFGContext& tctx, ebm::StatementRef io_ref, std::vector<Route>& finalized_routes, size_t max_bit_size, bool write) {
        auto& ctx = tctx.tctx.context();
        ebm::Block block;
        EBMU_U8_N_ARRAY(max_buffer_t, max_bit_size / 8, write ? ebm::ArrayAnnotation::write_temporary : ebm::ArrayAnnotation::read_temporary);
        EBMU_COUNTER_TYPE(count_t);
        EBM_DEFAULT_VALUE(default_buf_v, max_buffer_t);
        EBMU_BOOL_TYPE(bool_t);
        EBMU_U8(u8_t);
        EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_buffer, max_buffer_t, default_buf_v);
        EBMU_INT_LITERAL(zero, 0);
        EBM_DEFINE_ANONYMOUS_VARIABLE(current_bit_offset, count_t, zero);
        EBM_DEFINE_ANONYMOUS_VARIABLE(read_offset, count_t, zero);

        ebm::StatementRef flush_buffer_statement;
        if (write) {  // incremental reserving
            MAYBE(new_id, ctx.repository().new_statement_id());
            flush_buffer_statement = new_id;
        }

        // added_bit = current_bit_offset + add_bit
        // new_size = (added_bit + 7) / 8
        // for read_offset < new_size:
        //    tmp_buffer[read_offset] = read u8
        //    read_offset++
        auto do_incremental = [&](ebm::StatementRef field, ebm::ExpressionRef added_bit) -> expected<ebm::StatementRef> {
            EBMU_INT_LITERAL(seven, 7);
            EBMU_INT_LITERAL(eight, 8);
            EBM_BINARY_OP(new_size_bit, ebm::BinaryOp::add, count_t, added_bit, seven);
            EBM_BINARY_OP(new_size, ebm::BinaryOp::div, count_t, new_size_bit, eight);
            EBM_BINARY_OP(condition, ebm::BinaryOp::less, bool_t, read_offset, new_size);
            if (write) {  // incremental reserving
                ebm::ReserveData reserve_data;
                reserve_data.write_data = to_weak(flush_buffer_statement);
                MAYBE(reserve_size, make_dynamic_size(new_size, ebm::SizeUnit::BYTE_DYNAMIC));
                reserve_data.size = reserve_size;
                EBM_RESERVE_DATA(reserve_stmt, std::move(reserve_data));
                return reserve_stmt;
            }
            ebm::Block block;
            {
                EBM_INDEX(indexed, u8_t, tmp_buffer, read_offset);
                auto data = make_io_data(io_ref, field, indexed, u8_t, {}, get_size(8));
                MAYBE(lowered, ctx.get_decoder_converter().decode_multi_byte_int_with_fixed_array(io_ref, field, 1, {}, indexed, u8_t));
                data.attribute.has_lowered_statement(true);
                data.lowered_statement(make_lowered_statement(ebm::LoweringIOType::INT_TO_BYTE_ARRAY, lowered));
                EBM_READ_DATA(read_to_temporary, std::move(data));
                append(block, read_to_temporary);
                EBM_INCREMENT(inc, read_offset, count_t);
                append(block, inc);
            }
            EBM_BLOCK(read, std::move(block));
            EBM_WHILE_LOOP(loop, condition, read);
            EBM_BINARY_OP(len, ebm::BinaryOp::sub, count_t, new_size, read_offset);
            auto data = make_io_data(io_ref, field, tmp_buffer, max_buffer_t, {}, *make_dynamic_size(len, ebm::SizeUnit::BYTE_DYNAMIC));
            MAYBE(offset, make_dynamic_size(read_offset, ebm::SizeUnit::BYTE_DYNAMIC));
            data.attribute.has_offset(true);
            data.offset(offset);
            data.attribute.has_lowered_statement(true);
            data.lowered_statement(make_lowered_statement(ebm::LoweringIOType::ARRAY_FOR_EACH, loop));
            EBM_READ_DATA(read_data, std::move(data));
            return read_data;
        };
        auto flush_buffer = [&](ebm::StatementRef field) -> expected<ebm::StatementRef> {
            EBMU_INT_LITERAL(eight, 8);
            EBM_BINARY_OP(div, ebm::BinaryOp::div, count_t, current_bit_offset, eight);
            MAYBE(cur_offset, make_dynamic_size(div, ebm::SizeUnit::BYTE_DYNAMIC));
            auto write_buffer = make_io_data(io_ref, field, tmp_buffer, max_buffer_t, {}, cur_offset);
            auto write_data = make_write_data(std::move(write_buffer));
            EBMA_ADD_STATEMENT(flush_stmt, flush_buffer_statement, std::move(write_data));
            return flush_stmt;
        };
        std::set<std::shared_ptr<CFG>> reached_route;
        for (auto& r : finalized_routes) {
            BitManipulator extractor(ctx, tmp_buffer, u8_t);
            for (size_t i = 0; i < r.route.size(); i++) {
                auto& c = r.route[i];
                if (reached_route.contains(c)) {
                    continue;
                }
                reached_route.insert(c);
                MAYBE(stmt, tctx.tctx.statement_repository().get(c->original_node));
                auto io_ = get_io(stmt, write);
                if (io_) {
                    auto io_copy = *io_;  // to avoid memory location movement
                    auto add_bit = io_copy.size.size()->value();
                    if (io_copy.size.unit == ebm::SizeUnit::BYTE_FIXED) {
                        add_bit *= 8;
                    }
                    EBMU_INT_LITERAL(bit_size, add_bit);
                    EBM_BINARY_OP(new_size_bit, ebm::BinaryOp::add, count_t, current_bit_offset, bit_size);
                    EBM_ASSIGNMENT(update_current_bit_offset, current_bit_offset, new_size_bit);

                    EBMU_UINT_TYPE(unsigned_t, add_bit);
                    ebm::Block block;
                    if (i == 0) {
                        append(block, tmp_buffer_def);
                        append(block, current_bit_offset_def);
                        if (!write) {
                            append(block, read_offset_def);
                        }
                    }
                    if (!write) {
                        MAYBE(io_cond, do_incremental(from_weak(io_copy.field), new_size_bit));
                        EBM_DEFAULT_VALUE(zero, unsigned_t);
                        EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_holder, unsigned_t, zero);
                        auto assign = add_endian_specific(
                            ctx, io_copy.attribute,
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.read_bits_dynamic(current_bit_offset, bit_size, ebm::Endian::little, unsigned_t, tmp_holder);
                            },
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.read_bits_dynamic(current_bit_offset, bit_size, ebm::Endian::big, unsigned_t, tmp_holder);
                            });
                        if (!assign) {
                            return unexpect_error(std::move(assign.error()));
                        }
                        EBM_CAST(casted, io_copy.data_type, unsigned_t, tmp_holder);
                        EBM_ASSIGNMENT(fin, io_copy.target, casted);
                        append(block, tmp_holder_def);
                        append(block, io_cond);
                        append(block, *assign);
                        append(block, fin);
                    }
                    else {
                        MAYBE(incremental_reserve, do_incremental(from_weak(io_copy.field), new_size_bit));
                        EBM_CAST(casted, unsigned_t, io_copy.data_type, io_copy.target);
                        auto assign = add_endian_specific(
                            ctx, io_copy.attribute,
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.write_bits_dynamic(current_bit_offset, bit_size, ebm::Endian::little, unsigned_t, casted);
                            },
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.write_bits_dynamic(current_bit_offset, bit_size, ebm::Endian::big, unsigned_t, casted);
                            });
                        if (!assign) {
                            return unexpect_error(std::move(assign.error()));
                        }
                        append(block, incremental_reserve);
                        append(block, *assign);
                    }
                    append(block, update_current_bit_offset);
                    if (write && i == r.route.size() - 1) {
                        MAYBE(flush, flush_buffer(from_weak(io_copy.field)));
                        append(block, flush);
                    }
                    EBM_BLOCK(lowered_bit_operation, std::move(block));
                    MAYBE_VOID(add, add_lowered_statements(tctx, io_copy, c->original_node, lowered_bit_operation, write));
                }
            }
        }
        return {};
    }

    // single route
    // a - b - c - d
    // or
    // a - b - c
    //   \ - d (not merged finally)
    expected<void> single_route(CFGContext& tctx, ebm::StatementRef io_ref, std::vector<Route>& finalized_routes, size_t max_bit_size, bool write) {
        auto& ctx = tctx.tctx.context();
        ebm::Block block;
        EBMU_U8_N_ARRAY(max_buffer_t, max_bit_size / 8, write ? ebm::ArrayAnnotation::write_temporary : ebm::ArrayAnnotation::read_temporary);
        // EBMU_COUNTER_TYPE(count_t);
        EBM_DEFAULT_VALUE(default_buf_v, max_buffer_t);
        // EBMU_BOOL_TYPE(bool_t);
        EBMU_U8(u8_t);
        EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_buffer, max_buffer_t, default_buf_v);
        // EBMU_INT_LITERAL(zero, 0);
        //  EBM_DEFINE_ANONYMOUS_VARIABLE(current_bit_offset, count_t, zero);
        //  EBM_DEFINE_ANONYMOUS_VARIABLE(read_offset, count_t, zero);

        auto do_read = [&](ebm::StatementRef field, size_t& read_offset, size_t new_size) -> expected<ebm::StatementRef> {
            ebm::Block block;
            size_t original_read_offset = read_offset;
            while (read_offset < new_size) {
                EBMU_INT_LITERAL(read_offset_expr, read_offset);
                EBM_INDEX(indexed, u8_t, tmp_buffer, read_offset_expr);
                auto data = make_io_data(io_ref, field, indexed, u8_t, {}, get_size(8));
                MAYBE(lowered, ctx.get_decoder_converter().decode_multi_byte_int_with_fixed_array(io_ref, field, 1, {}, indexed, u8_t));
                data.attribute.has_lowered_statement(true);
                data.lowered_statement(make_lowered_statement(ebm::LoweringIOType::INT_TO_BYTE_ARRAY, lowered));
                EBM_READ_DATA(read_to_temporary, std::move(data));
                append(block, read_to_temporary);
                read_offset++;
            }
            EBM_BLOCK(read, std::move(block));
            auto data = make_io_data(io_ref, field, tmp_buffer, max_buffer_t, {}, get_size((new_size - original_read_offset) * 8));
            MAYBE(offset_lit, make_fixed_size(original_read_offset, ebm::SizeUnit::BYTE_FIXED));
            data.attribute.has_offset(true);
            data.offset(offset_lit);
            data.attribute.has_lowered_statement(true);
            data.lowered_statement(make_lowered_statement(ebm::LoweringIOType::ARRAY_FOR_EACH, read));
            EBM_READ_DATA(read_data, std::move(data));
            return read_data;
        };

        ebm::StatementRef flush_buffer_statement;
        bool is_single_route = finalized_routes.size() == 1;
        ebm::StatementRef initial_reserve_stmt;
        if (write) {  // incremental reserving
            MAYBE(new_id, ctx.repository().new_statement_id());
            flush_buffer_statement = new_id;
            if (is_single_route) {
                // reserve upfront for single route
                ebm::ReserveData reserve_data;
                reserve_data.write_data = to_weak(flush_buffer_statement);
                MAYBE(reserve_size, make_fixed_size(max_bit_size / 8, ebm::SizeUnit::BYTE_FIXED));
                reserve_data.size = reserve_size;
                EBM_RESERVE_DATA(reserve_stmt, std::move(reserve_data));
                initial_reserve_stmt = reserve_stmt;
            }
        }
        else {
            if (is_single_route) {
                // read upfront for single route
                size_t read_offset = 0;
                MAYBE(read_data, do_read(finalized_routes[0].route[0]->original_node, read_offset, max_bit_size / 8));
                initial_reserve_stmt = read_data;
            }
        }

        // added_bit = current_bit_offset + add_bit
        // new_size = (added_bit + 7) / 8
        // for read_offset < new_size:
        //    tmp_buffer[read_offset] = read u8
        //    read_offset++
        auto do_incremental = [&](ebm::StatementRef field, size_t& read_offset, size_t added_bit) -> expected<ebm::StatementRef> {
            // EBMU_INT_LITERAL(seven, 7);
            // EBMU_INT_LITERAL(eight, 8);
            // EBM_BINARY_OP(new_size_bit, ebm::BinaryOp::add, count_t, added_bit, seven);
            // EBM_BINARY_OP(new_size, ebm::BinaryOp::div, count_t, new_size_bit, eight);
            // EBM_BINARY_OP(condition, ebm::BinaryOp::less, bool_t, read_offset, new_size);
            auto new_size = (added_bit + 7) / 8;
            if (read_offset >= new_size) {
                return ebm::StatementRef{};
            }
            if (is_single_route) {
                return ebm::StatementRef{};  // already reserved upfront
            }
            if (write) {  // incremental reserving
                ebm::ReserveData reserve_data;
                reserve_data.write_data = ebm::WeakStatementRef{flush_buffer_statement};
                MAYBE(reserve_size, make_fixed_size(new_size, ebm::SizeUnit::BYTE_FIXED));
                reserve_data.size = reserve_size;
                EBM_RESERVE_DATA(reserve_stmt, std::move(reserve_data));
                return reserve_stmt;
            }
            /*
            ebm::Block block;
            size_t original_read_offset = read_offset;
            while (read_offset < new_size) {
                EBMU_INT_LITERAL(read_offset_expr, read_offset);
                EBM_INDEX(indexed, u8_t, tmp_buffer, read_offset_expr);
                auto data = make_io_data(io_ref, field, indexed, u8_t, {}, get_size(8));
                MAYBE(lowered, ctx.get_decoder_converter().decode_multi_byte_int_with_fixed_array(io_ref, field, 1, {}, indexed, u8_t));
                data.attribute.has_lowered_statement(true);
                data.lowered_statement(make_lowered_statement(ebm::LoweringIOType::INT_TO_BYTE_ARRAY, lowered));
                EBM_READ_DATA(read_to_temporary, std::move(data));
                append(block, read_to_temporary);
                read_offset++;
            }
            EBM_BLOCK(read, std::move(block));
            auto data = make_io_data(io_ref, field, tmp_buffer, max_buffer_t, {}, get_size((new_size - original_read_offset) * 8));
            EBMU_INT_LITERAL(offset_lit, original_read_offset);
            data.attribute.has_offset(true);
            data.offset(offset_lit);
            data.attribute.has_lowered_statement(true);
            data.lowered_statement(make_lowered_statement(ebm::LoweringIOType::ARRAY_FOR_EACH, read));
            EBM_READ_DATA(read_data, std::move(data));
            return read_data;
            */
            return do_read(field, read_offset, new_size);
        };
        auto flush_buffer = [&](ebm::StatementRef field, size_t new_size_bit) -> expected<ebm::StatementRef> {
            // EBMU_INT_LITERAL(eight, 8);
            // EBM_BINARY_OP(div, ebm::BinaryOp::div, count_t, new_size_bit, eight);
            MAYBE(cur_offset, make_fixed_size(new_size_bit / 8, ebm::SizeUnit::BYTE_FIXED));
            auto write_buffer = make_io_data(io_ref, field, tmp_buffer, max_buffer_t, {}, cur_offset);
            auto write_data = make_write_data(std::move(write_buffer));
            EBMA_ADD_STATEMENT(flush_stmt, flush_buffer_statement, std::move(write_data));
            return flush_stmt;
        };
        std::set<std::shared_ptr<CFG>> reached_route;
        for (auto& r : finalized_routes) {
            BitManipulator extractor(ctx, tmp_buffer, u8_t);
            size_t current_bit_offset = 0;
            size_t read_offset = 0;
            for (size_t i = 0; i < r.route.size(); i++) {
                auto& c = r.route[i];

                MAYBE(stmt, tctx.tctx.statement_repository().get(c->original_node));
                auto io_ = get_io(stmt, write);
                if (io_) {
                    auto io_copy = *io_;  // to avoid memory location movement
                    auto add_bit = io_copy.size.size()->value();
                    if (io_copy.size.unit == ebm::SizeUnit::BYTE_FIXED) {
                        add_bit *= 8;
                    }
                    // EBMU_INT_LITERAL(bit_size, add_bit);
                    auto bit_size = add_bit;
                    if (reached_route.contains(c)) {
                        current_bit_offset += bit_size;
                        read_offset = (current_bit_offset + 7) / 8;
                        continue;
                    }
                    reached_route.insert(c);
                    // EBM_BINARY_OP(new_size_bit, ebm::BinaryOp::add, count_t, current_bit_offset, bit_size);
                    // EBM_ASSIGNMENT(update_current_bit_offset, current_bit_offset, new_size_bit);
                    auto new_size_bit = current_bit_offset + bit_size;
                    EBMU_UINT_TYPE(unsigned_t, add_bit);
                    ebm::Block block;
                    if (i == 0) {
                        append(block, tmp_buffer_def);
                        if (!is_nil(initial_reserve_stmt)) {
                            append(block, initial_reserve_stmt);
                        }
                        // append(block, current_bit_offset_def);
                        // if (!write) {
                        //    append(block, read_offset_def);
                        //}
                    }
                    if (!write) {
                        MAYBE(io_cond, do_incremental(from_weak(io_copy.field), read_offset, new_size_bit));
                        EBM_DEFAULT_VALUE(zero, unsigned_t);
                        EBM_DEFINE_ANONYMOUS_VARIABLE(tmp_holder, unsigned_t, zero);
                        auto assign = add_endian_specific(
                            ctx, io_copy.attribute,
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.read_bits(current_bit_offset, bit_size, ebm::Endian::little, unsigned_t, tmp_holder);
                            },
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.read_bits(current_bit_offset, bit_size, ebm::Endian::big, unsigned_t, tmp_holder);
                            });
                        if (!assign) {
                            return unexpect_error(std::move(assign.error()));
                        }
                        EBM_CAST(casted, io_copy.data_type, unsigned_t, tmp_holder);
                        EBM_ASSIGNMENT(fin, io_copy.target, casted);
                        append(block, tmp_holder_def);
                        if (!is_nil(io_cond)) {
                            append(block, io_cond);
                        }
                        append(block, *assign);
                        append(block, fin);
                    }
                    else {
                        MAYBE(incremental_reserve, do_incremental(from_weak(io_copy.field), read_offset, new_size_bit));
                        EBM_CAST(casted, unsigned_t, io_copy.data_type, io_copy.target);
                        auto assign = add_endian_specific(
                            ctx, io_copy.attribute,
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.write_bits(current_bit_offset, bit_size, ebm::Endian::little, unsigned_t, casted);
                            },
                            [&] -> expected<ebm::StatementRef> {
                                return extractor.write_bits(current_bit_offset, bit_size, ebm::Endian::big, unsigned_t, casted);
                            });
                        if (!assign) {
                            return unexpect_error(std::move(assign.error()));
                        }
                        if (!is_nil(incremental_reserve)) {
                            append(block, incremental_reserve);
                        }
                        append(block, *assign);
                        if (i == r.route.size() - 1) {
                            MAYBE(flush, flush_buffer(from_weak(io_copy.field), new_size_bit));
                            append(block, flush);
                            // if other routes exist and this one is not last one,
                            // need new flush buffer statement id
                            if (finalized_routes.size() > 1 && &r != &finalized_routes.back()) {
                                MAYBE(new_id, ctx.repository().new_statement_id());
                                flush_buffer_statement = new_id;
                            }
                        }
                    }
                    // append(block, update_current_bit_offset);
                    current_bit_offset = new_size_bit;
                    EBM_BLOCK(lowered_bit_operation, std::move(block));
                    MAYBE_VOID(add, add_lowered_statements(tctx, io_copy, c->original_node, lowered_bit_operation, write));
                }
            }
        }
        return {};
    }

    // true if:
    // a - b - c - d
    //   \- e - f - g
    // false if:
    // a - b - c  - d
    //   \- e - f -/
    bool is_finally_unmerged(const std::vector<Route>& finalized_routes) {
        size_t i = 0;
        // detect first divergence
        while (true) {
            bool break_outer = false;
            std::shared_ptr<CFG> node;
            for (auto& r : finalized_routes) {
                if (i >= r.route.size()) {
                    return false;  // unexpected end
                }
                if (!node) {
                    node = r.route[i];
                }
                else if (node != r.route[i]) {
                    break_outer = true;
                    break;
                }
            }
            if (break_outer) {
                break;
            }
            if (!node) {
                return false;
            }
            i++;
        }
        // check routes after divergence are not merged again
        std::unordered_set<std::shared_ptr<CFG>> visited;
        for (auto& r : finalized_routes) {
            for (size_t j = i; j < r.route.size(); j++) {
                auto& c = r.route[j];
                if (visited.contains(c)) {
                    return false;
                }
                visited.insert(c);
            }
        }
        return true;
    }

    expected<void> add_lowered_bit_io(CFGContext& tctx, ebm::StatementRef io_ref, std::vector<Route>& finalized_routes, bool write) {
        print_if_verbose("Found ", finalized_routes.size(), " routes for ", write ? "write" : "read", "\n");
        size_t max_bit_size = 0;
        for (auto& r : finalized_routes) {
            print_if_verbose("  - ", r.route.size(), " node with ", r.bit_size, " bit\n");
            max_bit_size = (std::max)(max_bit_size, r.bit_size);
        }
        if (finalized_routes.size() == 1 || is_finally_unmerged(finalized_routes)) {
            MAYBE_VOID(ok, single_route(tctx, io_ref, finalized_routes, max_bit_size, write));
            return {};
        }
        MAYBE_VOID(ok, multi_route_merged(tctx, io_ref, finalized_routes, max_bit_size, write));
        return {};
    }

    expected<void> lowered_dynamic_bit_io(CFGContext& tctx, bool write) {
        auto& ctx = tctx.tctx.context();
        auto& all_statements = tctx.tctx.statement_repository().get_all();
        auto current_added = all_statements.size();

        for (size_t i = 0; i < current_added; ++i) {
            auto block = get_block(all_statements[i].body);
            if (!block) {
                continue;
            }
            std::set<std::shared_ptr<CFG>> handled;
            for (auto& ref : block->container) {
                MAYBE(stmt, tctx.tctx.statement_repository().get(ref));
                if (auto r = get_io(stmt, write); r && r->size.unit == ebm::SizeUnit::BIT_FIXED) {
                    auto found = tctx.stack.cfg_map.find(get_id(stmt.id));
                    if (found == tctx.stack.cfg_map.end()) {
                        return unexpect_error("no cfg found for {}:{}", get_id(stmt.id), to_string(stmt.body.kind));
                    }
                    if (handled.contains(found->second)) {
                        continue;
                    }
                    auto finalized_routes = search_byte_aligned_route(tctx, found->second, r->size.size()->value(), write);
                    if (finalized_routes.size()) {
                        MAYBE_VOID(added, add_lowered_bit_io(tctx, r->io_ref, finalized_routes, write));
                        for (auto& fin : finalized_routes) {
                            for (auto& node : fin.route) {
                                handled.insert(node);
                            }
                        }
                    }
                }
            }
        }
        return {};
    }

}  // namespace ebmgen