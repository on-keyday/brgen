/*license*/
#pragma once
#include <string>
#include <code/code_writer.h>
#include <binary/writer.h>
#include <unordered_map>
#include <bm/binary_module.hpp>
#include <format>
#include <bmgen/helper.hpp>
#include "output.hpp"

namespace bm2 {
    using TmpCodeWriter = futils::code::CodeWriter<std::string>;

    struct BitFieldState {
        std::uint64_t ident;
        std::uint64_t bit_size;
        rebgn::PackedOpType op;
        rebgn::EndianExpr endian;
    };

    struct Context {
        futils::code::CodeWriter<futils::binary::writer&> cw;
        const rebgn::BinaryModule& bm;
        Output& output;
        std::unordered_map<std::uint64_t, std::string> string_table;
        std::unordered_map<std::uint64_t, std::string> ident_table;
        std::unordered_map<std::uint64_t, std::uint64_t> ident_index_table;
        std::unordered_map<std::uint64_t, rebgn::Range> ident_range_table;
        std::unordered_map<std::uint64_t, std::string> metadata_table;
        std::unordered_map<std::uint64_t, rebgn::Storages> storage_table;
        std::string ptr_type;
        rebgn::BMContext bm_ctx;
        std::vector<futils::helper::DynDefer> on_functions;
        std::vector<std::string> this_as;
        std::vector<BitFieldState> bit_field_ident;

        std::vector<std::string> current_r;
        std::vector<std::string> current_w;
        std::vector<std::string> current_offset;

        bool on_assign = false;

        std::string root_r = "r";
        std::string root_w = "w";
        std::string root_offset = "*offset";
        std::string root_this = "(*this)";

        std::string r() {
            if (current_r.empty()) {
                return root_r;
            }
            return current_r.back();
        }

        std::string offset() {
            if (current_offset.empty()) {
                return root_offset;
            }
            return current_offset.back();
        }

        std::string w() {
            if (current_w.empty()) {
                return root_w;
            }
            return current_w.back();
        }

        std::string this_() {
            if (this_as.empty()) {
                return root_this;
            }
            return this_as.back();
        }

        const rebgn::Code& ref(rebgn::Varint ident) {
            return bm.code[ident_index_table[ident.value()]];
        }

        std::string ident(rebgn::Varint ident) {
            auto& ref = ident_table[ident.value()];
            if (ref.empty()) {
                ref = std::format("tmp{}", ident.value());
            }
            return ref;
        }

        rebgn::Range range(rebgn::Varint ident) {
            return ident_range_table[ident.value()];
        }

        Context(futils::binary::writer& w, const rebgn::BinaryModule& bm, bm2::Output& output,
                std::string_view root_r, std::string_view root_w, std::string_view root_this, auto&& escape_ident)
            : cw(w), bm(bm), output(output), root_r(root_r), root_w(root_w), root_this(root_this) {
            auto& ctx = *this;
            for (auto& sr : bm.strings.refs) {
                ctx.string_table[sr.code.value()] = sr.string.data;
            }
            for (auto& id : bm.ident_indexes.refs) {
                ctx.ident_index_table[id.ident.value()] = id.index.value();
            }
            for (auto& id : bm.ident_ranges.ranges) {
                ctx.ident_range_table[id.ident.value()] = rebgn::Range{.start = id.range.start.value(), .end = id.range.end.value()};
                auto& code = bm.code[id.range.start.value()];
            }
            for (auto& md : bm.metadata.refs) {
                ctx.metadata_table[md.code.value()] = md.string.data;
            }
            for (auto& st : bm.types.maps) {
                ctx.storage_table[st.code.value()] = st.storage;
            }
            for (auto& ir : bm.identifiers.refs) {
                ctx.ident_table[ir.code.value()] = ir.string.data;  // temporary assign
            }
            for (auto& ir : bm.identifiers.refs) {
                escape_ident(ctx, ir.code.value(), ctx.ident_table[ir.code.value()]);
                static_assert(std::is_void_v<std::invoke_result_t<decltype(escape_ident), Context&, std::uint64_t, std::string&>>);
            }
        }

        friend std::optional<size_t> find_op(Context& ctx, rebgn::Range range, rebgn::AbstractOp op, size_t from = 0) {
            if (from == 0) {
                from = range.start;
            }
            for (size_t i = from; i < range.end; i++) {
                if (ctx.bm.code[i].op == op) {
                    return i;
                }
            }
            return std::nullopt;
        }

        friend std::vector<size_t> find_ops(Context& ctx, rebgn::Range range, rebgn::AbstractOp op) {
            std::vector<size_t> res;
            for (size_t i = range.start; i < range.end; i++) {
                if (ctx.bm.code[i].op == op) {
                    res.push_back(i);
                }
            }
            return res;
        }

        friend std::optional<size_t> find_belong_op(Context& ctx, const rebgn::Code& code, rebgn::AbstractOp op) {
            if (code.op == op) {
                return code.ident().value().value();
            }
            if (auto belong = code.belong(); belong) {
                auto idx = ctx.ident_index_table[belong.value().value()];
                return find_belong_op(ctx, ctx.bm.code[idx], op);
            }
            return std::nullopt;
        }

        friend size_t find_next_else_or_end_if(Context& ctx, size_t start, bool include_else = false) {
            size_t nested = 0;
            for (size_t i = start + 1;; i++) {
                if (i >= ctx.bm.code.size()) {
                    return ctx.bm.code.size();
                }
                auto& code = ctx.bm.code[i];
                if (nested) {
                    if (code.op == rebgn::AbstractOp::END_IF) {
                        nested--;
                    }
                }
                else {
                    if (!include_else) {
                        if (code.op == rebgn::AbstractOp::END_IF) {
                            return i;
                        }
                    }
                    else {
                        if (code.op == rebgn::AbstractOp::ELIF || code.op == rebgn::AbstractOp::ELSE || code.op == rebgn::AbstractOp::END_IF) {
                            return i;
                        }
                    }
                }
                if (code.op == rebgn::AbstractOp::IF) {
                    nested++;
                }
            }
        }

        friend size_t find_next_end_loop(Context& ctx, size_t start) {
            size_t nested = 0;
            for (size_t i = start + 1;; i++) {
                if (i >= ctx.bm.code.size()) {
                    return ctx.bm.code.size();
                }
                auto& code = ctx.bm.code[i];
                if (nested) {
                    if (code.op == rebgn::AbstractOp::END_LOOP) {
                        nested--;
                    }
                }
                else {
                    if (code.op == rebgn::AbstractOp::END_LOOP) {
                        return i;
                    }
                }
                if (code.op == rebgn::AbstractOp::LOOP_INFINITE || code.op == rebgn::AbstractOp::LOOP_CONDITION) {
                    nested++;
                }
            }
        }

        friend std::vector<size_t> get_parameters(Context& ctx, rebgn::Range func_range) {
            std::vector<size_t> res;
            for (size_t i = func_range.start; i < func_range.end; i++) {
                auto& code = ctx.bm.code[i];
                if (code.op != rebgn::AbstractOp::RETURN_TYPE &&
                    rebgn::is_parameter_related(code.op)) {
                    res.push_back(i);
                }
            }
            return res;
        }
    };

    constexpr std::uint64_t null_ref = 0;

}  // namespace bm2
