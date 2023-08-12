/*license*/
#pragma once
#include <optional>

namespace brgen::ast {
    enum class UnaryOp {
        logical_not = 0,
        bit_not,
        increment,
        decrement,
        minus_sign,
        deref,
        address,
        post_increment,
        post_decrement,
    };

    constexpr const char* unary_op[] = {"!", "~", "++", "--", "-", "*", "&", nullptr};
    enum class BinaryOp {
        // layer 1
        mul,
        div,
        mod,
        left_shift,
        right_shift,
        bit_and,

        // layer 2
        add,
        sub,
        bit_or,
        bit_xor,

        // layer 3
        equal,
        not_equal,
        less,
        less_or_eq,
        grater,
        grater_or_eq,

        // layer 4
        logical_and,

        // layer 5
        logical_or,

        // layer 6
        cond_op1,
        cond_op2,

        // layer 7
        assign,
        typed_assign,
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

    constexpr const char* bin_layer0[] = {"*", "/", "%", "<<", ">>", "&", nullptr};
    constexpr const char* bin_layer1[] = {"+", "-", "|", "^", nullptr};
    constexpr const char* bin_layer2[] = {"==", "!=", "<", "<=", ">", ">=", nullptr};
    constexpr const char* bin_layer3[] = {"&&", nullptr};
    constexpr const char* bin_layer4[] = {"||", nullptr};
    constexpr const char* bin_layer5[] = {"if", "else", nullptr};
    constexpr const char* bin_layer6[] = {"=", ":=", "::=", "+=", "-=", "*=",
                                          "/=", "%=", "<<=", ">>=",
                                          "&=", "|=", "^=",
                                          nullptr};
    constexpr const char* bin_layer7[] = {",", nullptr};

    constexpr auto bin_cond_layer = 5;
    constexpr auto bin_assign_layer = 6;
    constexpr auto bin_comma_layer = 7;

    constexpr const char* const* bin_layers[] = {
        bin_layer0,
        bin_layer1,
        bin_layer2,
        bin_layer3,
        bin_layer4,
        bin_layer5,
        bin_layer6,
        bin_layer7,
    };

    constexpr size_t bin_layer_len = sizeof(bin_layers) / sizeof(bin_layers[0]);

    constexpr std::optional<BinaryOp> bin_op(const char* l) {
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