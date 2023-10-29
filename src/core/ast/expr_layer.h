/*license*/
#pragma once
#include <optional>
#include <string_view>

namespace brgen::ast {
    enum class UnaryOp {
        not_ = 0,
        minus_sign,
    };

    constexpr const char* unary_op_str[] = {"!", "-", nullptr};
    constexpr const char* unary_op_name[] = {"not", "minus_sign", nullptr};

    constexpr auto unary_op_count = 2;

    constexpr std::optional<UnaryOp> unary_op(std::string_view l) {
        size_t i = 0;
        while (unary_op_str[i]) {
            if (l == unary_op_str[i]) {
                return UnaryOp(i);
            }
            i++;
        }
        return std::nullopt;
    }

    enum class BinaryOp {
        // layer 0
        mul,
        div,
        mod,
        left_arithmetic_shift,
        right_arithmetic_shift,
        left_logical_shift,
        right_logical_shift,
        bit_and,

        // layer 1
        add,
        sub,
        bit_or,
        bit_xor,

        // layer 2
        equal,
        not_equal,
        less,
        less_or_eq,
        grater,
        grater_or_eq,

        // layer 3
        logical_and,

        // layer 4
        logical_or,

        // layer 5
        cond_op1,
        cond_op2,

        // layer 6
        range_exclusive,
        range_inclusive,

        // layer 7
        assign,
        define_assign,
        const_assign,
        add_assign,
        sub_assign,
        mul_assign,
        div_assign,
        mod_assign,
        left_shift_assign,
        right_shift_assign,
        bit_and_assign,
        bit_or_assign,
        bit_xor_assign,

        // layer 8
        comma,
    };

    constexpr const char* bin_layer0[] = {"*", "/", "%", "<<<", ">>>", "<<", ">>", "&", nullptr};
    constexpr const char* bin_layer1[] = {"+", "-", "|", "^", nullptr};
    constexpr const char* bin_layer2[] = {"==", "!=", "<", "<=", ">", ">=", nullptr};
    constexpr const char* bin_layer3[] = {"&&", nullptr};
    constexpr const char* bin_layer4[] = {"||", nullptr};
    constexpr const char* bin_layer5[] = {"?", ":", nullptr};
    constexpr const char* bin_layer6[] = {"..", "..=", nullptr};
    constexpr const char* bin_layer7[] = {"=", ":=", "::=", "+=", "-=", "*=",
                                          "/=", "%=", "<<=", ">>=",
                                          "&=", "|=", "^=",
                                          nullptr};
    constexpr const char* bin_layer8[] = {",", nullptr};

    // clang-format off
    constexpr const char* bin_op_list[] = {
        "*", "/", "%","<<<",">>>", "<<", ">>", "&",
        "+", "-", "|", "^",
        "==", "!=", "<", "<=", ">", ">=",
        "&&",
        "||",
        "?", ":",
        "..", "..=",
        "=", ":=", "::=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=",
        ",",
        nullptr,
    };

    constexpr const char* bin_op_name[]={
        "mul", "div", "mod", "left_arithmetic_shift", "right_arithmetic_shift", "left_logical_shift", "right_logical_shift","bit_and",
        "add", "sub", "bit_or", "bit_xor",
        "equal", "not_equal", "less", "less_or_eq", "grater", "grater_or_eq",
        "logical_and",
        "logical_or",
        "cond_op1", "cond_op2",
        "range_exclusive", "range_inclusive",
        "assign", "define_assign", "const_assign", "add_assign", "sub_assign", "mul_assign", "div_assign", "mod_assign", "left_shift_assign", "right_shift_assign", "bit_and_assign", "bit_or_assign", "bit_xor_assign",
        "comma",
        nullptr,
    };
    // clang-format on

    constexpr auto bin_op_count = std::size(bin_op_list) - 1;

    constexpr auto bin_cond_layer = 5;
    constexpr auto bin_assign_layer = 7;
    constexpr auto bin_comma_layer = 8;
    constexpr auto bin_compare_layer = 2;
    constexpr auto bin_range_layer = 6;

    constexpr bool is_compare_op(BinaryOp op) {
        constexpr auto begin = int(BinaryOp::equal);
        constexpr auto end = int(BinaryOp::grater_or_eq);
        return begin <= int(op) && int(op) <= end;
    }

    constexpr bool is_range_op(BinaryOp op) {
        constexpr auto begin = int(BinaryOp::range_inclusive);
        constexpr auto end = int(BinaryOp::range_exclusive);
        return begin <= int(op) && int(op) <= end;
    }

    constexpr bool is_boolean_op(BinaryOp op) {
        constexpr auto begin = int(BinaryOp::equal);
        constexpr auto end = int(BinaryOp::logical_or);
        return begin <= int(op) && int(op) <= end;
    }

    constexpr const char* const* bin_layers[] = {
        bin_layer0,
        bin_layer1,
        bin_layer2,
        bin_layer3,
        bin_layer4,
        bin_layer5,
        bin_layer6,
        bin_layer7,
        bin_layer8,
    };

    constexpr size_t bin_layer_len = sizeof(bin_layers) / sizeof(bin_layers[0]);

    constexpr std::optional<BinaryOp> bin_op(std::string_view l) {
        size_t i = 0;
        for (auto& layer : bin_layers) {
            size_t j = 0;
            while (layer[j]) {
                if (layer[j] == l) {
                    return BinaryOp(i);
                }
                j++;
                i++;
            }
        }
        return std::nullopt;
    }

    constexpr std::optional<const char*> bin_op_str(BinaryOp op) {
        size_t i = 0;
        for (auto& layer : bin_layers) {
            size_t j = 0;
            while (layer[j]) {
                if (BinaryOp(i) == op) {
                    return layer[j];
                }
                j++;
                i++;
            }
        }
        return std::nullopt;
    }

}  // namespace brgen::ast
