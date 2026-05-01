/*license*/
#pragma once
#include "../codegen.hpp"

namespace CODEGEN_NAMESPACE {

template <typename Ctx>
expected<std::string> type_to_pack_format(Ctx& ctx, const ebm::TypeRef& type_ref, const ebm::IOAttribute& attribute, const ebm::Size& size) {
    MAYBE(type, ctx.get(type_ref));

    std::string endian_suffix;
    switch (attribute.endian()) {
        case ebm::Endian::little:
            endian_suffix = "<";
            break;
        case ebm::Endian::big:
        case ebm::Endian::unspec:
            endian_suffix = ">";
            break;
        case ebm::Endian::native:
            endian_suffix = "";
            break;
        case ebm::Endian::dynamic:
            return unexpect_error("Dynamic endianness not supported in Ruby pack format.");
    }

    switch (type.body.kind) {
        case ebm::TypeKind::INT:
        case ebm::TypeKind::UINT: {
            if (!size.size()) {
                return unexpect_error("Integer type requires a fixed size for pack format.");
            }
            auto int_size = size.size()->value();
            if (size.unit == ebm::SizeUnit::BYTE_FIXED) {
                if (int_size == 1) {
                    return attribute.sign() ? std::string("\"c\"") : std::string("\"C\"");
                }
                std::string fmt_char;
                if (int_size == 2) {
                    fmt_char = attribute.sign() ? "s" : "S";
                }
                else if (int_size == 4) {
                    fmt_char = attribute.sign() ? "l" : "L";
                }
                else if (int_size == 8) {
                    fmt_char = attribute.sign() ? "q" : "Q";
                }
                else {
                    return unexpect_error("Unsupported integer size for pack format: {} bytes", int_size);
                }
                return std::format("\"{}{}\"", fmt_char, endian_suffix);
            }
            else if (size.unit == ebm::SizeUnit::BIT_FIXED) {
                return unexpect_error("Bit field integer types not supported in Ruby pack format.");
            }
            return unexpect_error("Unsupported size unit for integer type: {}", to_string(size.unit));
        }
        case ebm::TypeKind::FLOAT: {
            if (!size.size()) {
                return unexpect_error("Float type requires a fixed size for pack format.");
            }
            auto float_size = size.size()->value();
            if (size.unit == ebm::SizeUnit::BYTE_FIXED) {
                switch (attribute.endian()) {
                    case ebm::Endian::little:
                        if (float_size == 4) return std::string("\"e\"");
                        if (float_size == 8) return std::string("\"E\"");
                        return unexpect_error("Unsupported float size for little-endian pack format: {} bytes", float_size);
                    case ebm::Endian::big:
                    case ebm::Endian::unspec:
                        if (float_size == 4) return std::string("\"g\"");
                        if (float_size == 8) return std::string("\"G\"");
                        return unexpect_error("Unsupported float size for big-endian pack format: {} bytes", float_size);
                    case ebm::Endian::native:
                        if (float_size == 4) return std::string("\"f\"");
                        if (float_size == 8) return std::string("\"d\"");
                        return unexpect_error("Unsupported float size for native pack format: {} bytes", float_size);
                    case ebm::Endian::dynamic:
                        return unexpect_error("Dynamic endianness not supported in Ruby pack format.");
                }
            }
            return unexpect_error("Unsupported size unit for float type: {}", to_string(size.unit));
        }
        default:
            return unexpect_error("Unhandled TypeKind for Ruby pack format: {}", to_string(type.body.kind));
    }
}

}  // namespace CODEGEN_NAMESPACE
