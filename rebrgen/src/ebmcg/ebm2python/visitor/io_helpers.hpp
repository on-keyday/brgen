/*license*/
#pragma once
#include "../codegen.hpp"

namespace CODEGEN_NAMESPACE {

template <typename Ctx>
expected<std::string> type_to_struct_format(Ctx& ctx, const ebm::TypeRef& type_ref, const ebm::IOAttribute& attribute, const ebm::Size& size) {
    std::string endian_prefix;
    switch (attribute.endian()) {
        case ebm::Endian::little:
            endian_prefix = "<";
            break;
        case ebm::Endian::big:
            endian_prefix = ">";
            break;
        case ebm::Endian::native:
            endian_prefix = "@";
            break;
        case ebm::Endian::unspec:
            endian_prefix = "!";
            break;
        case ebm::Endian::dynamic:
            return unexpect_error("Dynamic endianness not supported in struct format string generation.");
    }

    MAYBE(type, ctx.get(type_ref));

    std::string format_char;
    switch (type.body.kind) {
        case ebm::TypeKind::INT:
        case ebm::TypeKind::UINT: {
            if (!size.size()) {
                return unexpect_error("Integer type requires a fixed size for struct format.");
            }
            auto int_size = size.size()->value();
            if (size.unit == ebm::SizeUnit::BYTE_FIXED) {
                if (int_size == 1)
                    format_char = attribute.sign() ? "b" : "B";
                else if (int_size == 2)
                    format_char = attribute.sign() ? "h" : "H";
                else if (int_size == 4)
                    format_char = attribute.sign() ? "i" : "I";
                else if (int_size == 8)
                    format_char = attribute.sign() ? "q" : "Q";
                else
                    return unexpect_error("Unsupported integer size for struct format: {} bytes", int_size);
            }
            else if (size.unit == ebm::SizeUnit::BIT_FIXED) {
                return unexpect_error("Bit field integer types not supported in struct format string generation.");
            }
            else {
                return unexpect_error("Unsupported size unit for integer type: {}", to_string(size.unit));
            }
            break;
        }
        case ebm::TypeKind::FLOAT: {
            if (!size.size()) {
                return unexpect_error("Float type requires a fixed size for struct format.");
            }
            auto float_size = size.size()->value();
            if (size.unit == ebm::SizeUnit::BYTE_FIXED) {
                if (float_size == 4)
                    format_char = "f";
                else if (float_size == 8)
                    format_char = "d";
                else
                    return unexpect_error("Unsupported float size for struct format: {} bytes", float_size);
            }
            else {
                return unexpect_error("Unsupported size unit for float type: {}", to_string(size.unit));
            }
            break;
        }
        case ebm::TypeKind::ARRAY:
        case ebm::TypeKind::VECTOR: {
            MAYBE(element_type, ctx.get(*type.body.element_type()));
            if ((element_type.body.kind == ebm::TypeKind::UINT || element_type.body.kind == ebm::TypeKind::INT) &&
                element_type.body.size() && element_type.body.size()->value() == 8) {
                if (auto s = size.size()) {
                    format_char = std::to_string(s->value()) + "s";
                }
                else if (auto r = size.ref()) {
                    MAYBE(dyn_size, ctx.visit(*r));
                    format_char = std::format("\" + f\"{{({})}}s", dyn_size.to_string());
                }
            }
            else {
                return unexpect_error("Unsupported array/vector type for struct format string generation: {} {}", to_string(element_type.body.kind), to_string(size.unit));
            }
            break;
        }
        default:
            return unexpect_error("Unhandled TypeKind for struct format: {}", to_string(type.body.kind));
    }

    return std::format("\"{}{}\"", endian_prefix, format_char);
}


// ADR 0039: true when the io stream's input type is flagged has_absolute_offset,
// i.e. lower_runtime_state threaded a RuntimeState companion through the
// enclosing function (parameter/local name is always `runtime_state`).
inline bool py_has_absolute_offset(auto& ctx, ebm::StatementRef io_ref) {
    auto typ = ctx.template get_field<"param_decl.param_type">(io_ref);
    if (!typ) {
        typ = ctx.template get_field<"var_decl.var_type">(io_ref);
    }
    return ctx.template get_field<"io_input_desc.has_absolute_offset">(typ) == true;
}

// Emit `runtime_state.offset += <size>` when the stream is gated. The increment
// stays backend-side per ADR 0008/0039; BytesIO's cursor stays stream-local,
// the companion counts absolute bytes.
inline void py_append_runtime_offset(auto& ctx, ebm::StatementRef io_ref, CodeWriter& w, auto&& size_expr) {
    if (py_has_absolute_offset(ctx, io_ref)) {
        w.writeln("runtime_state.offset += int(", size_expr, ")");
    }
}

}  // namespace CODEGEN_NAMESPACE
