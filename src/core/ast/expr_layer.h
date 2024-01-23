/*license*/
#pragma once
#include <optional>
#include <string_view>
#include "node/ast_enum.h"
#include <view/span.h>

namespace brgen::ast {

    // clang-format off
    constexpr const char* bin_layer0[] = {"*", "/", "%", "<<<", ">>>", "<<", ">>", "&"};
    constexpr const char* bin_layer1[] = {"+", "-", "|", "^"};
    constexpr const char* bin_layer2[] = {"==", "!=", "<", "<=", ">", ">="};
    constexpr const char* bin_layer3[] = {"&&"};
    constexpr const char* bin_layer4[] = {"||"};
    constexpr const char* bin_layer5[] = {"?", ":"};
    constexpr const char* bin_layer6[] = {"..", "..="};
    constexpr const char* bin_layer7[] = {"=", ":=", "::=", "+=", "-=", "*=",
                                          "/=", "%=", "<<=", ">>=",
                                          "&=", "|=", "^=",
                                         };
    constexpr const char* bin_layer8[] = {","};



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

    constexpr bool is_assign_op(BinaryOp op) {
        constexpr auto begin = int(BinaryOp::assign);
        constexpr auto end = int(BinaryOp::bit_xor_assign);
        return begin <= int(op) && int(op) <= end;
    }

    constexpr std::array<futils::view::rspan<const char* const>, 9> bin_layers = {
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

    namespace test {
        constexpr bool check_layers() {
            size_t seq = 0;
            constexpr auto a = enum_array<BinaryOp>;
            for (size_t i = 0; i < bin_layer_len; i++) {
                for (auto j = 0; j < bin_layers[i].size(); j++) {
                    if (a[seq].first != BinaryOp(seq)) {
                        [](auto...) { throw "error"; }(a[seq].first, BinaryOp(seq));
                        return false;
                    }
                    if (a[seq].second != bin_layers[i][j]) {
                        [](auto...) { throw "error"; }(a[seq].second, bin_layers[i][j]);
                        return false;
                    }
                    seq++;
                }
            }
            if (seq != a.size()) {
                [](auto...) { throw "error"; }(seq, bin_op_count);
                return false;
            }
            return true;
        }

        static_assert(check_layers());
    }  // namespace test

}  // namespace brgen::ast
