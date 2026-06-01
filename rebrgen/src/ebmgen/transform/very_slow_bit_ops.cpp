/*license*/
#pragma once
#include "../converter.hpp"
#include "../visitor/visitor.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmcodegen/stub/make_visitor.hpp"
#include "ebmgen/access.hpp"
#include "ebmgen/mapping.hpp"
#include "to_mapping_table.hpp"
namespace ebmgen {

    expected<void> convert_encode_bit_op_to_very_slow(TransformContext& tctx, ebm::StatementRef stmt_ref) {
        auto& ctx = tctx.context();
        MAYBE(stmt, tctx.statement_repository().get(stmt_ref));
        MAYBE(write_data, stmt.body.write_data());
        MAYBE(kind, access_field<"kind">(ctx.repository(), write_data.data_type));
        print_if_verbose("very slow bit io(write): ", to_string(kind), "\n");
        return {};
    }

    expected<void> convert_decode_bit_op_to_very_slow(TransformContext& tctx, ebm::StatementRef stmt_ref) {
        auto& ctx = tctx.context();
        MAYBE(stmt, tctx.statement_repository().get(stmt_ref));
        MAYBE(read_data, stmt.body.read_data());
        MAYBE(kind, access_field<"kind">(ctx.repository(), read_data.data_type));
        print_if_verbose("very slow bit io(read): ", to_string(kind), "\n");
        // input state
        //   bit_offset = 0~7
        //   current_byte = 0x00~0xFF
        // field state
        //   bit_size
        //   target
        // endianness/bit order state
        //   is_little_endian
        //   stream_is_lsb_first
        //   mapping_is_lsb_first
        //
        // is_little_endian - targetへbit値をどちら方向(lsb or msb)からマップしていくか
        // stream_is_lsb_first - byte alignedした地点からまずどちらから読むか
        // mapping_is_lsb_first - 取得したbitをどちらからマップするか
        //
        // for i = 0; i < bit_size; :
        //   if bit_offset == 0:
        //     current_byte = read_byte()
        //   remaining_bits = 8 - bit_offset
        //   shift = bit_offset if shift_from_lsb else (7 - bit_offset)
        //   current_bit = (current_byte >> shift) & 1
        //   target_shift = i if is_little_endian else (bit_size - 1 - i)
        //   if i == 0:
        //      target = 0
        //   target |= current_bit << target_shift
        //   bit_offset++
        //   bit_offset &= 7

        // TODO
        return {};
    }

    expected<void> derive_very_slow_bit_ops(TransformContext& tctx) {
        auto& ctx = tctx.context();
        std::vector<size_t> bit_op_stmt_index;
        for (size_t i = 0; i < tctx.statement_repository().get_all().size(); ++i) {
            const auto& stmt = tctx.statement_repository().get_all()[i];
            if (auto wd = stmt.body.func_decl();
                wd && (wd->kind == ebm::FunctionKind::ENCODE || wd->kind == ebm::FunctionKind::DECODE) &&
                !wd->attribute.is_wrapper()) {
                bit_op_stmt_index.push_back(i);
            }
        }
        visitor::BaseVisitor visitor{to_mapping_table(tctx)};
        visitor.module_.build_maps(mapping::BuildMapOption::NONE);
        visitor::InitialContext ictx{.visitor = visitor};

        std::vector<ebm::StatementRef> write_data_refs;
        std::vector<ebm::StatementRef> read_data_refs;
        auto top_level_io_collector = visitor::make_visitor<void>(ictx.visitor)
                                          .not_before_or_after()
                                          .not_context("Type")
                                          .on([&](auto&& self, visitor::Context_Statement_WRITE_DATA& ctx) -> expected<void> {
                                              write_data_refs.push_back(ctx.item_id);
                                              return {};
                                          })
                                          .on([&](auto&& self, visitor::Context_Statement_READ_DATA& ctx) -> expected<void> {
                                              read_data_refs.push_back(ctx.item_id);
                                              return {};
                                          })
                                          .on_default_traverse_children()
                                          .build();

        for (auto& index : bit_op_stmt_index) {
            auto& stmt = tctx.statement_repository().get_all()[index];
            MAYBE(func_decl, stmt.body.func_decl());
            MAYBE_VOID(ok, ictx.visit(top_level_io_collector, func_decl.body));
        }

        std::vector<ebm::StatementRef> terminal_write_data_refs;
        std::vector<ebm::StatementRef> terminal_read_data_refs;

        auto lowering_collector = visitor::make_visitor<void>(ictx.visitor)
                                      .not_before_or_after()
                                      .not_context("Type")
                                      .on([&](auto&& self, visitor::Context_Statement_WRITE_DATA& ctx) -> expected<void> {
                                          if (auto lowered = ctx.write_data.lowered_statement()) {
                                              return ctx.visit(self, lowered->io_statement.id);
                                          }
                                          terminal_write_data_refs.push_back(ctx.item_id);
                                          return {};
                                      })
                                      .on([&](auto&& self, visitor::Context_Statement_READ_DATA& ctx) -> expected<void> {
                                          if (auto lowered = ctx.read_data.lowered_statement()) {
                                              return ctx.visit<void>(self, lowered->io_statement.id);
                                          }
                                          terminal_read_data_refs.push_back(ctx.item_id);
                                          return {};
                                      })
                                      .on_default_traverse_children()
                                      .build();

        for (auto& ref : write_data_refs) {
            MAYBE_VOID(ok, ictx.visit(lowering_collector, ref));
        }
        for (auto& ref : read_data_refs) {
            MAYBE_VOID(ok, ictx.visit(lowering_collector, ref));
        }

        for (auto& stmt_ref : terminal_write_data_refs) {
            MAYBE_VOID(ok, convert_encode_bit_op_to_very_slow(tctx, stmt_ref));
        }

        for (auto& stmt_ref : terminal_read_data_refs) {
            MAYBE_VOID(ok, convert_decode_bit_op_to_very_slow(tctx, stmt_ref));
        }
        return {};
    }

}  // namespace ebmgen