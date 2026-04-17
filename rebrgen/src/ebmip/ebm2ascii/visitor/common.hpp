/*license*/
#pragma once
#include "../codegen.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmcodegen/stub/util.hpp"

namespace ebm2ascii {

    struct TypeInfo {
        std::uint64_t bit_width = 0;  // 0 if unknown/variable
        std::string repr;
        bool is_variable = false;
        std::string length_expr;  // for variable arrays
    };

    struct Field {
        std::string name;
        std::uint64_t bit_offset = 0;
        std::uint64_t bit_width = 0;  // 0 if variable
        std::string type_repr;
        bool is_variable = false;
        std::string length_expr;
    };

    struct Diagram {
        std::string name;
        std::vector<Field> fields;
        std::uint64_t total_bits = 0;
        bool has_variable = false;
    };

    struct TypeAnalyzer {
        TRAVERSAL_VISITOR_BASE_WITHOUT_FUNC(TypeAnalyzer, BaseVisitor);

        expected<TypeInfo> visit(Context_Type_INT& ctx) {
            auto sz = ctx.size.value();
            return TypeInfo{sz, std::format("i{}", sz), false, ""};
        }
        expected<TypeInfo> visit(Context_Type_UINT& ctx) {
            auto sz = ctx.size.value();
            return TypeInfo{sz, std::format("u{}", sz), false, ""};
        }
        expected<TypeInfo> visit(Context_Type_FLOAT& ctx) {
            auto sz = ctx.size.value();
            return TypeInfo{sz, std::format("f{}", sz), false, ""};
        }
        expected<TypeInfo> visit(Context_Type_BOOL& ctx) {
            return TypeInfo{1, "bool", false, ""};
        }
        expected<TypeInfo> visit(Context_Type_ENUM& ctx) {
            auto name = ctx.identifier(ctx.id);
            if (auto* stmt = visitor.module_.get_statement(ctx.id)) {
                if (auto* ed = stmt->body.enum_decl()) {
                    MAYBE(base, ctx.visit<TypeInfo>(*this, ed->base_type));
                    if (!base.is_variable && base.bit_width > 0) {
                        return TypeInfo{base.bit_width, std::format("{}({})", name, base.repr), false, ""};
                    }
                }
            }
            return TypeInfo{0, name, true, ""};
        }
        expected<TypeInfo> visit(Context_Type_STRUCT& ctx) {
            auto name = ctx.identifier(ctx.id);
            if (auto* stmt = visitor.module_.get_statement(ctx.id)) {
                if (auto* sd = stmt->body.struct_decl()) {
                    if (sd->is_fixed_size()) {
                        if (auto* sz = sd->size()) {
                            if (auto* v = sz->size()) {
                                std::uint64_t bits = 0;
                                if (sz->unit == ebm::SizeUnit::BIT_FIXED) {
                                    bits = v->value();
                                }
                                else if (sz->unit == ebm::SizeUnit::BYTE_FIXED) {
                                    bits = v->value() * 8;
                                }
                                if (bits > 0) {
                                    return TypeInfo{bits, name, false, ""};
                                }
                            }
                        }
                    }
                }
            }
            return TypeInfo{0, name, true, ""};
        }
        expected<TypeInfo> visit(Context_Type_RECURSIVE_STRUCT& ctx) {
            auto name = ctx.identifier(ctx.id);
            return TypeInfo{0, name, true, ""};
        }
        expected<TypeInfo> visit(Context_Type_ARRAY& ctx) {
            MAYBE(elem, ctx.visit<TypeInfo>(*this, ctx.element_type));
            auto len = ctx.length.value();
            std::uint64_t total = (elem.bit_width > 0) ? elem.bit_width * len : 0;
            return TypeInfo{total, std::format("[{}]{}", len, elem.repr), elem.is_variable, ""};
        }
        expected<TypeInfo> visit(Context_Type_VECTOR& ctx) {
            MAYBE(elem, ctx.visit<TypeInfo>(*this, ctx.element_type));
            return TypeInfo{0, std::format("[]{}", elem.repr), true, ""};
        }

        template <typename Ctx>
        expected<TypeInfo> visit(Ctx&& ctx) {
            if (ctx.is_before_or_after()) {
                return pass;
            }
            if (ctx.context_name.contains("Statement") ||
                ctx.context_name.contains("Expression")) {
                return {};
            }
            return traverse_children<TypeInfo>(*this, std::forward<Ctx>(ctx));
        }
    };

    // Minimal Expression → string renderer for simple literal / enum value contexts.
    // Not comprehensive — falls back to "<expr>" for unhandled expression kinds.
    struct ExprStringer {
        TRAVERSAL_VISITOR_BASE_WITHOUT_FUNC(ExprStringer, BaseVisitor);

        expected<std::string> visit(Context_Expression_LITERAL_INT& ctx) {
            return std::format("{}", ctx.int_value.value());
        }
        expected<std::string> visit(Context_Expression_LITERAL_INT64& ctx) {
            return std::format("{}", ctx.int64_value);
        }
        expected<std::string> visit(Context_Expression_LITERAL_BOOL& ctx) {
            return ctx.bool_value ? std::string("true") : std::string("false");
        }
        expected<std::string> visit(Context_Expression_BINARY_OP& ctx) {
            MAYBE(left, ctx.visit<std::string>(*this, ctx.left));
            MAYBE(right, ctx.visit<std::string>(*this, ctx.right));
            return std::format("{} {} {}", left, to_string(ctx.bop), right);
        }
        expected<std::string> visit(Context_Expression_UNARY_OP& ctx) {
            MAYBE(right, ctx.visit<std::string>(*this, ctx.operand));
            return std::format("{}{}", to_string(ctx.uop), right);
        }

        template <typename Ctx>
        expected<std::string> visit(Ctx&& ctx) {
            if (ctx.is_before_or_after()) {
                return pass;
            }
            if (ctx.context_name.contains("Statement") ||
                ctx.context_name.contains("Type")) {
                return {};
            }
            return traverse_children<std::string>(*this, std::forward<Ctx>(ctx));
        }
    };

    struct EnumEntry {
        std::string name;
        std::string value;
    };

    struct EnumDiagram {
        std::string name;
        std::string base_repr;  // e.g. "u8"; empty if no base type resolvable
        std::vector<EnumEntry> entries;
    };

    inline std::string render_enum_ascii(const EnumDiagram& e) {
        std::string out;
        out += std::format("Enum: {}", e.name);
        if (!e.base_repr.empty()) {
            out += std::format(" ({})", e.base_repr);
        }
        out += "\n";
        for (const auto& m : e.entries) {
            out += std::format("  {} = {}\n", m.name, m.value);
        }
        return out;
    }

    inline std::string render_enum_table(const EnumDiagram& e) {
        std::string out;
        out += std::format("### Enum: {}", e.name);
        if (!e.base_repr.empty()) {
            out += std::format(" (`{}`)", e.base_repr);
        }
        out += "\n\n";
        out += "| Name | Value |\n|---|---:|\n";
        for (const auto& m : e.entries) {
            out += std::format("| `{}` | {} |\n", m.name, m.value);
        }
        return out;
    }

    struct FieldExtractor {
        TRAVERSAL_VISITOR_BASE_WITHOUT_FUNC(FieldExtractor, BaseVisitor);

        std::vector<Field> fields;
        std::uint64_t bit_offset = 0;
        bool has_variable = false;

        expected<void> visit(Context_Statement_FIELD_DECL& ctx) {
            TypeAnalyzer analyzer{ctx.visitor};
            MAYBE(info, ctx.visit<TypeInfo>(analyzer, ctx.field_decl.field_type));
            auto name = ctx.identifier();
            Field f{
                .name = name,
                .bit_offset = bit_offset,
                .bit_width = info.bit_width,
                .type_repr = info.repr,
                .is_variable = info.is_variable,
                .length_expr = info.length_expr,
            };
            fields.push_back(f);
            if (info.is_variable || info.bit_width == 0) {
                has_variable = true;
            }
            bit_offset += info.bit_width;
            return {};
        }

        template <typename Ctx>
        expected<void> visit(Ctx&& ctx) {
            if (ctx.is_before_or_after()) {
                return pass;
            }
            if (ctx.context_name.contains("Expression") ||
                ctx.context_name.contains("Type")) {
                return {};
            }
            return traverse_children<void>(*this, std::forward<Ctx>(ctx));
        }
    };

    // --- Rendering ---

    inline std::string render_ascii(const Diagram& d, std::uint32_t width_bits) {
        std::string out;
        out += std::format("Format: {}\n\n", d.name);

        // Header row with bit index numbers (tens + ones digits for 0..width-1)
        {
            std::string tens = " ";
            std::string ones = " ";
            for (std::uint32_t i = 0; i < width_bits; ++i) {
                tens += (i % 10 == 0) ? std::format("{}", i / 10) : " ";
                tens += " ";
                ones += std::format("{}", i % 10);
                ones += " ";
            }
            out += tens + "\n";
            out += ones + "\n";
        }

        // Separator line
        auto separator = [&]() {
            std::string s;
            s += "+";
            for (std::uint32_t i = 0; i < width_bits; ++i) {
                s += "-+";
            }
            s += "\n";
            return s;
        };

        out += separator();

        // Emit fields row-by-row
        std::uint64_t cursor = 0;
        std::string row;
        row += "|";
        std::uint32_t col = 0;

        auto flush_row = [&]() {
            // pad remaining columns
            while (col < width_bits) {
                row += "   ";  // 3 chars per cell (" ", val, "|"?) -- handled below
                col++;
            }
            row += "\n";
            out += row;
            out += separator();
            row.clear();
            row += "|";
            col = 0;
        };

        for (const auto& f : d.fields) {
            if (f.bit_width == 0) {
                // Variable-length: emit a "~" band for remainder of row and a free-form row
                if (col != 0) {
                    // finish current row first
                    while (col < width_bits) {
                        row += "   ";
                        col++;
                    }
                    row += "\n";
                    out += row;
                    out += separator();
                    row.clear();
                }
                // Emit a variable-length block
                std::string label;
                if (!f.length_expr.empty()) {
                    label = std::format("{} ({})", f.name, f.length_expr);
                }
                else {
                    label = std::format("{} (variable: {})", f.name, f.type_repr);
                }
                // Block width = total width in chars
                std::uint32_t total_chars = width_bits * 2 + 1;
                out += "|";
                std::uint32_t pad = (total_chars - 2 - (std::uint32_t)label.size()) / 2;
                for (std::uint32_t i = 0; i < pad; ++i) out += " ";
                out += label;
                std::uint32_t rem = total_chars - 2 - pad - (std::uint32_t)label.size();
                for (std::uint32_t i = 0; i < rem; ++i) out += " ";
                out += "|\n";
                // Rule with ~ for variable
                out += "~";
                for (std::uint32_t i = 0; i < width_bits; ++i) {
                    out += "~~";
                }
                out += "\n";
                // Reset row
                row = "|";
                col = 0;
                cursor += 0;  // unknown advance; approximate
                continue;
            }

            // Fixed-width field: draw a cell spanning f.bit_width columns
            std::uint64_t remaining = f.bit_width;
            while (remaining > 0) {
                std::uint64_t in_row = std::min<std::uint64_t>(remaining, width_bits - col);
                // Cell width in chars: in_row * 2 - 1 (each bit = 2 chars, minus final '|' which is cell delimiter)
                std::uint32_t cell_chars = (std::uint32_t)in_row * 2 - 1;
                // Choose label with fallback: "name (type)" → "name" → truncated name
                std::string cell_label;
                if (remaining == f.bit_width) {
                    auto full = std::format("{} ({})", f.name, f.type_repr);
                    if (full.size() <= cell_chars) {
                        cell_label = std::move(full);
                    }
                    else if (f.name.size() <= cell_chars) {
                        cell_label = f.name;
                    }
                    else {
                        cell_label = f.name.substr(0, cell_chars);
                    }
                }
                else {
                    auto cont = std::format("..{}", f.name);
                    cell_label = cont.size() <= cell_chars ? cont : cont.substr(0, cell_chars);
                }
                std::uint32_t pad = (cell_chars - (std::uint32_t)cell_label.size()) / 2;
                for (std::uint32_t i = 0; i < pad; ++i) row += " ";
                row += cell_label;
                std::uint32_t rem_chars = cell_chars - pad - (std::uint32_t)cell_label.size();
                for (std::uint32_t i = 0; i < rem_chars; ++i) row += " ";
                row += "|";
                col += (std::uint32_t)in_row;
                remaining -= in_row;
                cursor += in_row;
                if (col == width_bits) {
                    row += "\n";
                    out += row;
                    out += separator();
                    row.clear();
                    row += "|";
                    col = 0;
                }
            }
        }

        // Flush any in-progress row
        if (col != 0) {
            // Pad rest
            while (col < width_bits) {
                row += " ";
                // a single bit cell is 1 char content + '|' delimiter at end (2 chars per bit)
                row += " ";
                col++;
            }
            // Replace last " " with "|" if needed — but for MVP just append final "|"
            row += "\n";
            out += row;
            out += separator();
        }

        if (d.has_variable) {
            out += "\n(Note: total size is variable)\n";
        }
        else {
            out += std::format("\n(Total: {} bits)\n", d.total_bits);
        }

        return out;
    }

    inline std::string render_table(const Diagram& d) {
        std::string out;
        out += std::format("### Format: {}\n\n", d.name);
        out += "| Offset (bit) | Width (bit) | Name | Type |\n";
        out += "|---:|---:|---|---|\n";
        for (const auto& f : d.fields) {
            std::string width_cell = f.bit_width == 0 ? "var" : std::format("{}", f.bit_width);
            std::string offset_cell = std::format("{}", f.bit_offset);
            out += std::format("| {} | {} | `{}` | `{}` |\n", offset_cell, width_cell, f.name, f.type_repr);
        }
        if (d.has_variable) {
            out += "\n_Total size is variable._\n";
        }
        else {
            out += std::format("\n_Total: {} bits._\n", d.total_bits);
        }
        return out;
    }

}  // namespace ebm2ascii
