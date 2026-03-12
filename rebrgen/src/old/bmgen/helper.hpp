/*license*/
#pragma once
#include <bm/binary_module.hpp>
#include <format>

namespace rebgn {

    inline bool is_declare(AbstractOp op) {
        switch (op) {
            case AbstractOp::DECLARE_FUNCTION:
            case AbstractOp::DECLARE_ENUM:
            case AbstractOp::DECLARE_FORMAT:
            case AbstractOp::DECLARE_UNION:
            case AbstractOp::DECLARE_UNION_MEMBER:
            case AbstractOp::DECLARE_PROGRAM:
            case AbstractOp::DECLARE_STATE:
            case AbstractOp::DECLARE_BIT_FIELD:
                return true;
            default:
                return false;
        }
    }

    inline std::string storage_key(const Storages& s) {
        std::string key;
        for (auto& storage : s.storages) {
            key += to_string(storage.type);
            if (auto ref = storage.ref()) {
                key += std::format("r{}", ref.value().value());
            }
            if (auto size = storage.size()) {
                key += std::format("s{}", size.value().value());
            }
        }
        return key;
    }

    inline bool is_both_expr_and_def(AbstractOp op) {
        switch (op) {
            case AbstractOp::DEFINE_VARIABLE:
            case AbstractOp::DEFINE_CONSTANT:
            case AbstractOp::ASSIGN:
            case AbstractOp::DECLARE_VARIABLE:
                return true;
            default:
                return false;
        }
    }

    inline bool is_expr(AbstractOp op) {
        switch (op) {
            case AbstractOp::IMMEDIATE_INT:
            case AbstractOp::IMMEDIATE_CHAR:
            case AbstractOp::IMMEDIATE_STRING:
            case AbstractOp::IMMEDIATE_TRUE:
            case AbstractOp::IMMEDIATE_FALSE:
            case AbstractOp::IMMEDIATE_INT64:
            case AbstractOp::IMMEDIATE_TYPE:
            case AbstractOp::BINARY:
            case AbstractOp::UNARY:
            case AbstractOp::CAN_READ:
            case AbstractOp::IS_LITTLE_ENDIAN:
            case AbstractOp::CAST:
            case AbstractOp::CALL_CAST:
            case AbstractOp::ARRAY_SIZE:
            case AbstractOp::ADDRESS_OF:
            case AbstractOp::OPTIONAL_OF:
            case AbstractOp::EMPTY_PTR:
            case AbstractOp::EMPTY_OPTIONAL:
            case AbstractOp::FIELD_AVAILABLE:
            case AbstractOp::INPUT_BYTE_OFFSET:
            case AbstractOp::OUTPUT_BYTE_OFFSET:
            case AbstractOp::INPUT_BIT_OFFSET:
            case AbstractOp::OUTPUT_BIT_OFFSET:
            case AbstractOp::REMAIN_BYTES:
            case AbstractOp::PHI:
            case AbstractOp::ASSIGN:
            case AbstractOp::DEFINE_FIELD:
            case AbstractOp::DEFINE_BIT_FIELD:
            case AbstractOp::DEFINE_PROPERTY:
            case AbstractOp::DEFINE_VARIABLE:
            case AbstractOp::DECLARE_VARIABLE:
            case AbstractOp::DEFINE_VARIABLE_REF:
            case AbstractOp::DEFINE_CONSTANT:
            case AbstractOp::NEW_OBJECT:
            case AbstractOp::CALL:
            case AbstractOp::INDEX:
            case AbstractOp::ACCESS:
            case AbstractOp::BEGIN_COND_BLOCK:
            case AbstractOp::DEFINE_PARAMETER:
            case AbstractOp::PROPERTY_INPUT_PARAMETER:
                return true;
            default:
                return false;
        }
    }

    inline bool is_struct_define_related(AbstractOp op) {
        switch (op) {
            case AbstractOp::DEFINE_ENUM:
            case AbstractOp::DEFINE_ENUM_MEMBER:
            case AbstractOp::DECLARE_ENUM:
            case AbstractOp::DEFINE_FORMAT:
            case AbstractOp::DECLARE_FORMAT:
            case AbstractOp::DECLARE_UNION:
            case AbstractOp::DEFINE_UNION:
            case AbstractOp::DECLARE_UNION_MEMBER:
            case AbstractOp::DEFINE_UNION_MEMBER:
            case AbstractOp::DEFINE_STATE:
            case AbstractOp::DECLARE_STATE:
            case AbstractOp::DEFINE_BIT_FIELD:
            case AbstractOp::DECLARE_BIT_FIELD:
            case AbstractOp::DECLARE_FUNCTION:
            case AbstractOp::END_STATE:
            case AbstractOp::END_BIT_FIELD:
            case AbstractOp::END_ENUM:
            case AbstractOp::END_FORMAT:
            case AbstractOp::END_UNION:
            case AbstractOp::END_UNION_MEMBER:
            case AbstractOp::DEFINE_FIELD:
            case AbstractOp::DEFINE_PROPERTY:
            case AbstractOp::DECLARE_PROPERTY:
            case AbstractOp::END_PROPERTY:
            case AbstractOp::DEFINE_PROPERTY_GETTER:
            case AbstractOp::DEFINE_PROPERTY_SETTER:
                return true;
            default:
                return false;
        }
    }

    inline bool is_marker(AbstractOp op) {
        switch (op) {
            case AbstractOp::DEFINE_PROGRAM:
            case AbstractOp::END_PROGRAM:
            case AbstractOp::DECLARE_PROGRAM:
            case AbstractOp::CONDITIONAL_FIELD:
            case AbstractOp::CONDITIONAL_PROPERTY:
            case AbstractOp::MERGED_CONDITIONAL_FIELD:
            case AbstractOp::DEFINE_ENCODER:
            case AbstractOp::DEFINE_DECODER:
            case AbstractOp::NOT_PREV_THEN:
            case AbstractOp::DEFINE_FALLBACK:
            case AbstractOp::END_FALLBACK:
            case AbstractOp::PROPERTY_FUNCTION:
            case AbstractOp::END_COND_BLOCK:
                return true;
            default:
                return false;
        }
    }

    inline bool is_parameter_related(AbstractOp op) {
        switch (op) {
            case AbstractOp::RETURN_TYPE:
            case AbstractOp::ENCODER_PARAMETER:
            case AbstractOp::DECODER_PARAMETER:
            case AbstractOp::STATE_VARIABLE_PARAMETER:
            case AbstractOp::PROPERTY_INPUT_PARAMETER:
            case AbstractOp::DEFINE_PARAMETER:
                return true;
            default:
                return false;
        }
    }
}  // namespace rebgn
