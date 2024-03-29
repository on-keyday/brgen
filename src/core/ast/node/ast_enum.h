/*license*/
// Code generated by "enum_gen"; DO NOT EDIT.
#pragma once
#include <string_view>
#include <optional>
#include <array>
namespace brgen::ast {
template<typename T>
constexpr std::optional<T> from_string(std::string_view str);
template<typename T>
constexpr size_t enum_elem_count();
template<typename T>
constexpr std::array<std::pair<T,std::string_view>,enum_elem_count<T>()> make_enum_array();
template<typename T>
constexpr std::array<std::pair<T,std::string_view>,enum_elem_count<T>()> make_enum_name_array();
template<typename T>
constexpr const char* enum_type_name();
template<typename T>
constexpr auto enum_array = make_enum_array<T>();
template<typename T>
constexpr auto enum_name_array = make_enum_name_array<T>();
enum class Follow {
    unknown,
    end,
    fixed,
    constant,
    normal,
};
constexpr const char* to_string(Follow e) {
    switch(e) {
    case Follow::unknown: return "unknown";
    case Follow::end: return "end";
    case Follow::fixed: return "fixed";
    case Follow::constant: return "constant";
    case Follow::normal: return "normal";
    default: return nullptr;
    }
}
template<>constexpr std::optional<Follow> from_string<Follow>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "unknown") return Follow::unknown;
    if(str == "end") return Follow::end;
    if(str == "fixed") return Follow::fixed;
    if(str == "constant") return Follow::constant;
    if(str == "normal") return Follow::normal;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<Follow>() {
    return 5;
}
template<>constexpr std::array<std::pair<Follow,std::string_view>,5> make_enum_array<Follow>() {
    return {
        std::pair{Follow::unknown,"unknown"},
        std::pair{Follow::end,"end"},
        std::pair{Follow::fixed,"fixed"},
        std::pair{Follow::constant,"constant"},
        std::pair{Follow::normal,"normal"},
    };
}
template<>constexpr std::array<std::pair<Follow,std::string_view>,5> make_enum_name_array<Follow>() {
    return {
        std::pair{Follow::unknown,"unknown"},
        std::pair{Follow::end,"end"},
        std::pair{Follow::fixed,"fixed"},
        std::pair{Follow::constant,"constant"},
        std::pair{Follow::normal,"normal"},
    };
}
constexpr void as_json(Follow e,auto&& d) {
    d.value(enum_array<Follow>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<Follow>() {
    return "Follow";
}
enum class BinaryOp {
    mul,
    div,
    mod,
    left_arithmetic_shift,
    right_arithmetic_shift,
    left_logical_shift,
    right_logical_shift,
    bit_and,
    add,
    sub,
    bit_or,
    bit_xor,
    equal,
    not_equal,
    less,
    less_or_eq,
    grater,
    grater_or_eq,
    logical_and,
    logical_or,
    cond_op1,
    cond_op2,
    range_exclusive,
    range_inclusive,
    assign,
    define_assign,
    const_assign,
    add_assign,
    sub_assign,
    mul_assign,
    div_assign,
    mod_assign,
    left_logical_shift_assign,
    right_logical_shift_assign,
    left_arithmetic_shift_assign,
    right_arithmetic_shift_assign,
    bit_and_assign,
    bit_or_assign,
    bit_xor_assign,
    comma,
    in_assign,
};
constexpr const char* to_string(BinaryOp e) {
    switch(e) {
    case BinaryOp::mul: return "*";
    case BinaryOp::div: return "/";
    case BinaryOp::mod: return "%";
    case BinaryOp::left_arithmetic_shift: return "<<<";
    case BinaryOp::right_arithmetic_shift: return ">>>";
    case BinaryOp::left_logical_shift: return "<<";
    case BinaryOp::right_logical_shift: return ">>";
    case BinaryOp::bit_and: return "&";
    case BinaryOp::add: return "+";
    case BinaryOp::sub: return "-";
    case BinaryOp::bit_or: return "|";
    case BinaryOp::bit_xor: return "^";
    case BinaryOp::equal: return "==";
    case BinaryOp::not_equal: return "!=";
    case BinaryOp::less: return "<";
    case BinaryOp::less_or_eq: return "<=";
    case BinaryOp::grater: return ">";
    case BinaryOp::grater_or_eq: return ">=";
    case BinaryOp::logical_and: return "&&";
    case BinaryOp::logical_or: return "||";
    case BinaryOp::cond_op1: return "?";
    case BinaryOp::cond_op2: return ":";
    case BinaryOp::range_exclusive: return "..";
    case BinaryOp::range_inclusive: return "..=";
    case BinaryOp::assign: return "=";
    case BinaryOp::define_assign: return ":=";
    case BinaryOp::const_assign: return "::=";
    case BinaryOp::add_assign: return "+=";
    case BinaryOp::sub_assign: return "-=";
    case BinaryOp::mul_assign: return "*=";
    case BinaryOp::div_assign: return "/=";
    case BinaryOp::mod_assign: return "%=";
    case BinaryOp::left_logical_shift_assign: return "<<=";
    case BinaryOp::right_logical_shift_assign: return ">>=";
    case BinaryOp::left_arithmetic_shift_assign: return "<<<=";
    case BinaryOp::right_arithmetic_shift_assign: return ">>>=";
    case BinaryOp::bit_and_assign: return "&=";
    case BinaryOp::bit_or_assign: return "|=";
    case BinaryOp::bit_xor_assign: return "^=";
    case BinaryOp::comma: return ",";
    case BinaryOp::in_assign: return "in";
    default: return nullptr;
    }
}
template<>constexpr std::optional<BinaryOp> from_string<BinaryOp>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "*") return BinaryOp::mul;
    if(str == "/") return BinaryOp::div;
    if(str == "%") return BinaryOp::mod;
    if(str == "<<<") return BinaryOp::left_arithmetic_shift;
    if(str == ">>>") return BinaryOp::right_arithmetic_shift;
    if(str == "<<") return BinaryOp::left_logical_shift;
    if(str == ">>") return BinaryOp::right_logical_shift;
    if(str == "&") return BinaryOp::bit_and;
    if(str == "+") return BinaryOp::add;
    if(str == "-") return BinaryOp::sub;
    if(str == "|") return BinaryOp::bit_or;
    if(str == "^") return BinaryOp::bit_xor;
    if(str == "==") return BinaryOp::equal;
    if(str == "!=") return BinaryOp::not_equal;
    if(str == "<") return BinaryOp::less;
    if(str == "<=") return BinaryOp::less_or_eq;
    if(str == ">") return BinaryOp::grater;
    if(str == ">=") return BinaryOp::grater_or_eq;
    if(str == "&&") return BinaryOp::logical_and;
    if(str == "||") return BinaryOp::logical_or;
    if(str == "?") return BinaryOp::cond_op1;
    if(str == ":") return BinaryOp::cond_op2;
    if(str == "..") return BinaryOp::range_exclusive;
    if(str == "..=") return BinaryOp::range_inclusive;
    if(str == "=") return BinaryOp::assign;
    if(str == ":=") return BinaryOp::define_assign;
    if(str == "::=") return BinaryOp::const_assign;
    if(str == "+=") return BinaryOp::add_assign;
    if(str == "-=") return BinaryOp::sub_assign;
    if(str == "*=") return BinaryOp::mul_assign;
    if(str == "/=") return BinaryOp::div_assign;
    if(str == "%=") return BinaryOp::mod_assign;
    if(str == "<<=") return BinaryOp::left_logical_shift_assign;
    if(str == ">>=") return BinaryOp::right_logical_shift_assign;
    if(str == "<<<=") return BinaryOp::left_arithmetic_shift_assign;
    if(str == ">>>=") return BinaryOp::right_arithmetic_shift_assign;
    if(str == "&=") return BinaryOp::bit_and_assign;
    if(str == "|=") return BinaryOp::bit_or_assign;
    if(str == "^=") return BinaryOp::bit_xor_assign;
    if(str == ",") return BinaryOp::comma;
    if(str == "in") return BinaryOp::in_assign;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<BinaryOp>() {
    return 41;
}
template<>constexpr std::array<std::pair<BinaryOp,std::string_view>,41> make_enum_array<BinaryOp>() {
    return {
        std::pair{BinaryOp::mul,"*"},
        std::pair{BinaryOp::div,"/"},
        std::pair{BinaryOp::mod,"%"},
        std::pair{BinaryOp::left_arithmetic_shift,"<<<"},
        std::pair{BinaryOp::right_arithmetic_shift,">>>"},
        std::pair{BinaryOp::left_logical_shift,"<<"},
        std::pair{BinaryOp::right_logical_shift,">>"},
        std::pair{BinaryOp::bit_and,"&"},
        std::pair{BinaryOp::add,"+"},
        std::pair{BinaryOp::sub,"-"},
        std::pair{BinaryOp::bit_or,"|"},
        std::pair{BinaryOp::bit_xor,"^"},
        std::pair{BinaryOp::equal,"=="},
        std::pair{BinaryOp::not_equal,"!="},
        std::pair{BinaryOp::less,"<"},
        std::pair{BinaryOp::less_or_eq,"<="},
        std::pair{BinaryOp::grater,">"},
        std::pair{BinaryOp::grater_or_eq,">="},
        std::pair{BinaryOp::logical_and,"&&"},
        std::pair{BinaryOp::logical_or,"||"},
        std::pair{BinaryOp::cond_op1,"?"},
        std::pair{BinaryOp::cond_op2,":"},
        std::pair{BinaryOp::range_exclusive,".."},
        std::pair{BinaryOp::range_inclusive,"..="},
        std::pair{BinaryOp::assign,"="},
        std::pair{BinaryOp::define_assign,":="},
        std::pair{BinaryOp::const_assign,"::="},
        std::pair{BinaryOp::add_assign,"+="},
        std::pair{BinaryOp::sub_assign,"-="},
        std::pair{BinaryOp::mul_assign,"*="},
        std::pair{BinaryOp::div_assign,"/="},
        std::pair{BinaryOp::mod_assign,"%="},
        std::pair{BinaryOp::left_logical_shift_assign,"<<="},
        std::pair{BinaryOp::right_logical_shift_assign,">>="},
        std::pair{BinaryOp::left_arithmetic_shift_assign,"<<<="},
        std::pair{BinaryOp::right_arithmetic_shift_assign,">>>="},
        std::pair{BinaryOp::bit_and_assign,"&="},
        std::pair{BinaryOp::bit_or_assign,"|="},
        std::pair{BinaryOp::bit_xor_assign,"^="},
        std::pair{BinaryOp::comma,","},
        std::pair{BinaryOp::in_assign,"in"},
    };
}
template<>constexpr std::array<std::pair<BinaryOp,std::string_view>,41> make_enum_name_array<BinaryOp>() {
    return {
        std::pair{BinaryOp::mul,"mul"},
        std::pair{BinaryOp::div,"div"},
        std::pair{BinaryOp::mod,"mod"},
        std::pair{BinaryOp::left_arithmetic_shift,"left_arithmetic_shift"},
        std::pair{BinaryOp::right_arithmetic_shift,"right_arithmetic_shift"},
        std::pair{BinaryOp::left_logical_shift,"left_logical_shift"},
        std::pair{BinaryOp::right_logical_shift,"right_logical_shift"},
        std::pair{BinaryOp::bit_and,"bit_and"},
        std::pair{BinaryOp::add,"add"},
        std::pair{BinaryOp::sub,"sub"},
        std::pair{BinaryOp::bit_or,"bit_or"},
        std::pair{BinaryOp::bit_xor,"bit_xor"},
        std::pair{BinaryOp::equal,"equal"},
        std::pair{BinaryOp::not_equal,"not_equal"},
        std::pair{BinaryOp::less,"less"},
        std::pair{BinaryOp::less_or_eq,"less_or_eq"},
        std::pair{BinaryOp::grater,"grater"},
        std::pair{BinaryOp::grater_or_eq,"grater_or_eq"},
        std::pair{BinaryOp::logical_and,"logical_and"},
        std::pair{BinaryOp::logical_or,"logical_or"},
        std::pair{BinaryOp::cond_op1,"cond_op1"},
        std::pair{BinaryOp::cond_op2,"cond_op2"},
        std::pair{BinaryOp::range_exclusive,"range_exclusive"},
        std::pair{BinaryOp::range_inclusive,"range_inclusive"},
        std::pair{BinaryOp::assign,"assign"},
        std::pair{BinaryOp::define_assign,"define_assign"},
        std::pair{BinaryOp::const_assign,"const_assign"},
        std::pair{BinaryOp::add_assign,"add_assign"},
        std::pair{BinaryOp::sub_assign,"sub_assign"},
        std::pair{BinaryOp::mul_assign,"mul_assign"},
        std::pair{BinaryOp::div_assign,"div_assign"},
        std::pair{BinaryOp::mod_assign,"mod_assign"},
        std::pair{BinaryOp::left_logical_shift_assign,"left_logical_shift_assign"},
        std::pair{BinaryOp::right_logical_shift_assign,"right_logical_shift_assign"},
        std::pair{BinaryOp::left_arithmetic_shift_assign,"left_arithmetic_shift_assign"},
        std::pair{BinaryOp::right_arithmetic_shift_assign,"right_arithmetic_shift_assign"},
        std::pair{BinaryOp::bit_and_assign,"bit_and_assign"},
        std::pair{BinaryOp::bit_or_assign,"bit_or_assign"},
        std::pair{BinaryOp::bit_xor_assign,"bit_xor_assign"},
        std::pair{BinaryOp::comma,"comma"},
        std::pair{BinaryOp::in_assign,"in_assign"},
    };
}
constexpr void as_json(BinaryOp e,auto&& d) {
    d.value(enum_array<BinaryOp>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<BinaryOp>() {
    return "BinaryOp";
}
enum class UnaryOp {
    not_,
    minus_sign,
};
constexpr const char* to_string(UnaryOp e) {
    switch(e) {
    case UnaryOp::not_: return "!";
    case UnaryOp::minus_sign: return "-";
    default: return nullptr;
    }
}
template<>constexpr std::optional<UnaryOp> from_string<UnaryOp>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "!") return UnaryOp::not_;
    if(str == "-") return UnaryOp::minus_sign;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<UnaryOp>() {
    return 2;
}
template<>constexpr std::array<std::pair<UnaryOp,std::string_view>,2> make_enum_array<UnaryOp>() {
    return {
        std::pair{UnaryOp::not_,"!"},
        std::pair{UnaryOp::minus_sign,"-"},
    };
}
template<>constexpr std::array<std::pair<UnaryOp,std::string_view>,2> make_enum_name_array<UnaryOp>() {
    return {
        std::pair{UnaryOp::not_,"not"},
        std::pair{UnaryOp::minus_sign,"minus_sign"},
    };
}
constexpr void as_json(UnaryOp e,auto&& d) {
    d.value(enum_array<UnaryOp>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<UnaryOp>() {
    return "UnaryOp";
}
enum class BitAlignment {
    byte_aligned,
    bit_1,
    bit_2,
    bit_3,
    bit_4,
    bit_5,
    bit_6,
    bit_7,
    not_target,
    not_decidable,
};
constexpr const char* to_string(BitAlignment e) {
    switch(e) {
    case BitAlignment::byte_aligned: return "byte_aligned";
    case BitAlignment::bit_1: return "bit_1";
    case BitAlignment::bit_2: return "bit_2";
    case BitAlignment::bit_3: return "bit_3";
    case BitAlignment::bit_4: return "bit_4";
    case BitAlignment::bit_5: return "bit_5";
    case BitAlignment::bit_6: return "bit_6";
    case BitAlignment::bit_7: return "bit_7";
    case BitAlignment::not_target: return "not_target";
    case BitAlignment::not_decidable: return "not_decidable";
    default: return nullptr;
    }
}
template<>constexpr std::optional<BitAlignment> from_string<BitAlignment>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "byte_aligned") return BitAlignment::byte_aligned;
    if(str == "bit_1") return BitAlignment::bit_1;
    if(str == "bit_2") return BitAlignment::bit_2;
    if(str == "bit_3") return BitAlignment::bit_3;
    if(str == "bit_4") return BitAlignment::bit_4;
    if(str == "bit_5") return BitAlignment::bit_5;
    if(str == "bit_6") return BitAlignment::bit_6;
    if(str == "bit_7") return BitAlignment::bit_7;
    if(str == "not_target") return BitAlignment::not_target;
    if(str == "not_decidable") return BitAlignment::not_decidable;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<BitAlignment>() {
    return 10;
}
template<>constexpr std::array<std::pair<BitAlignment,std::string_view>,10> make_enum_array<BitAlignment>() {
    return {
        std::pair{BitAlignment::byte_aligned,"byte_aligned"},
        std::pair{BitAlignment::bit_1,"bit_1"},
        std::pair{BitAlignment::bit_2,"bit_2"},
        std::pair{BitAlignment::bit_3,"bit_3"},
        std::pair{BitAlignment::bit_4,"bit_4"},
        std::pair{BitAlignment::bit_5,"bit_5"},
        std::pair{BitAlignment::bit_6,"bit_6"},
        std::pair{BitAlignment::bit_7,"bit_7"},
        std::pair{BitAlignment::not_target,"not_target"},
        std::pair{BitAlignment::not_decidable,"not_decidable"},
    };
}
template<>constexpr std::array<std::pair<BitAlignment,std::string_view>,10> make_enum_name_array<BitAlignment>() {
    return {
        std::pair{BitAlignment::byte_aligned,"byte_aligned"},
        std::pair{BitAlignment::bit_1,"bit_1"},
        std::pair{BitAlignment::bit_2,"bit_2"},
        std::pair{BitAlignment::bit_3,"bit_3"},
        std::pair{BitAlignment::bit_4,"bit_4"},
        std::pair{BitAlignment::bit_5,"bit_5"},
        std::pair{BitAlignment::bit_6,"bit_6"},
        std::pair{BitAlignment::bit_7,"bit_7"},
        std::pair{BitAlignment::not_target,"not_target"},
        std::pair{BitAlignment::not_decidable,"not_decidable"},
    };
}
constexpr void as_json(BitAlignment e,auto&& d) {
    d.value(enum_array<BitAlignment>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<BitAlignment>() {
    return "BitAlignment";
}
enum class ConstantLevel {
    unknown,
    constant,
    immutable_variable,
    variable,
};
constexpr const char* to_string(ConstantLevel e) {
    switch(e) {
    case ConstantLevel::unknown: return "unknown";
    case ConstantLevel::constant: return "constant";
    case ConstantLevel::immutable_variable: return "immutable_variable";
    case ConstantLevel::variable: return "variable";
    default: return nullptr;
    }
}
template<>constexpr std::optional<ConstantLevel> from_string<ConstantLevel>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "unknown") return ConstantLevel::unknown;
    if(str == "constant") return ConstantLevel::constant;
    if(str == "immutable_variable") return ConstantLevel::immutable_variable;
    if(str == "variable") return ConstantLevel::variable;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<ConstantLevel>() {
    return 4;
}
template<>constexpr std::array<std::pair<ConstantLevel,std::string_view>,4> make_enum_array<ConstantLevel>() {
    return {
        std::pair{ConstantLevel::unknown,"unknown"},
        std::pair{ConstantLevel::constant,"constant"},
        std::pair{ConstantLevel::immutable_variable,"immutable_variable"},
        std::pair{ConstantLevel::variable,"variable"},
    };
}
template<>constexpr std::array<std::pair<ConstantLevel,std::string_view>,4> make_enum_name_array<ConstantLevel>() {
    return {
        std::pair{ConstantLevel::unknown,"unknown"},
        std::pair{ConstantLevel::constant,"constant"},
        std::pair{ConstantLevel::immutable_variable,"immutable_variable"},
        std::pair{ConstantLevel::variable,"variable"},
    };
}
constexpr void as_json(ConstantLevel e,auto&& d) {
    d.value(enum_array<ConstantLevel>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<ConstantLevel>() {
    return "ConstantLevel";
}
enum class IdentUsage {
    unknown,
    bad_ident,
    reference,
    define_variable,
    define_const,
    define_field,
    define_format,
    define_state,
    define_enum,
    define_enum_member,
    define_fn,
    define_cast_fn,
    define_arg,
    reference_type,
    reference_member,
    reference_member_type,
    maybe_type,
    reference_builtin_fn,
};
constexpr const char* to_string(IdentUsage e) {
    switch(e) {
    case IdentUsage::unknown: return "unknown";
    case IdentUsage::bad_ident: return "bad_ident";
    case IdentUsage::reference: return "reference";
    case IdentUsage::define_variable: return "define_variable";
    case IdentUsage::define_const: return "define_const";
    case IdentUsage::define_field: return "define_field";
    case IdentUsage::define_format: return "define_format";
    case IdentUsage::define_state: return "define_state";
    case IdentUsage::define_enum: return "define_enum";
    case IdentUsage::define_enum_member: return "define_enum_member";
    case IdentUsage::define_fn: return "define_fn";
    case IdentUsage::define_cast_fn: return "define_cast_fn";
    case IdentUsage::define_arg: return "define_arg";
    case IdentUsage::reference_type: return "reference_type";
    case IdentUsage::reference_member: return "reference_member";
    case IdentUsage::reference_member_type: return "reference_member_type";
    case IdentUsage::maybe_type: return "maybe_type";
    case IdentUsage::reference_builtin_fn: return "reference_builtin_fn";
    default: return nullptr;
    }
}
template<>constexpr std::optional<IdentUsage> from_string<IdentUsage>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "unknown") return IdentUsage::unknown;
    if(str == "bad_ident") return IdentUsage::bad_ident;
    if(str == "reference") return IdentUsage::reference;
    if(str == "define_variable") return IdentUsage::define_variable;
    if(str == "define_const") return IdentUsage::define_const;
    if(str == "define_field") return IdentUsage::define_field;
    if(str == "define_format") return IdentUsage::define_format;
    if(str == "define_state") return IdentUsage::define_state;
    if(str == "define_enum") return IdentUsage::define_enum;
    if(str == "define_enum_member") return IdentUsage::define_enum_member;
    if(str == "define_fn") return IdentUsage::define_fn;
    if(str == "define_cast_fn") return IdentUsage::define_cast_fn;
    if(str == "define_arg") return IdentUsage::define_arg;
    if(str == "reference_type") return IdentUsage::reference_type;
    if(str == "reference_member") return IdentUsage::reference_member;
    if(str == "reference_member_type") return IdentUsage::reference_member_type;
    if(str == "maybe_type") return IdentUsage::maybe_type;
    if(str == "reference_builtin_fn") return IdentUsage::reference_builtin_fn;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<IdentUsage>() {
    return 18;
}
template<>constexpr std::array<std::pair<IdentUsage,std::string_view>,18> make_enum_array<IdentUsage>() {
    return {
        std::pair{IdentUsage::unknown,"unknown"},
        std::pair{IdentUsage::bad_ident,"bad_ident"},
        std::pair{IdentUsage::reference,"reference"},
        std::pair{IdentUsage::define_variable,"define_variable"},
        std::pair{IdentUsage::define_const,"define_const"},
        std::pair{IdentUsage::define_field,"define_field"},
        std::pair{IdentUsage::define_format,"define_format"},
        std::pair{IdentUsage::define_state,"define_state"},
        std::pair{IdentUsage::define_enum,"define_enum"},
        std::pair{IdentUsage::define_enum_member,"define_enum_member"},
        std::pair{IdentUsage::define_fn,"define_fn"},
        std::pair{IdentUsage::define_cast_fn,"define_cast_fn"},
        std::pair{IdentUsage::define_arg,"define_arg"},
        std::pair{IdentUsage::reference_type,"reference_type"},
        std::pair{IdentUsage::reference_member,"reference_member"},
        std::pair{IdentUsage::reference_member_type,"reference_member_type"},
        std::pair{IdentUsage::maybe_type,"maybe_type"},
        std::pair{IdentUsage::reference_builtin_fn,"reference_builtin_fn"},
    };
}
template<>constexpr std::array<std::pair<IdentUsage,std::string_view>,18> make_enum_name_array<IdentUsage>() {
    return {
        std::pair{IdentUsage::unknown,"unknown"},
        std::pair{IdentUsage::bad_ident,"bad_ident"},
        std::pair{IdentUsage::reference,"reference"},
        std::pair{IdentUsage::define_variable,"define_variable"},
        std::pair{IdentUsage::define_const,"define_const"},
        std::pair{IdentUsage::define_field,"define_field"},
        std::pair{IdentUsage::define_format,"define_format"},
        std::pair{IdentUsage::define_state,"define_state"},
        std::pair{IdentUsage::define_enum,"define_enum"},
        std::pair{IdentUsage::define_enum_member,"define_enum_member"},
        std::pair{IdentUsage::define_fn,"define_fn"},
        std::pair{IdentUsage::define_cast_fn,"define_cast_fn"},
        std::pair{IdentUsage::define_arg,"define_arg"},
        std::pair{IdentUsage::reference_type,"reference_type"},
        std::pair{IdentUsage::reference_member,"reference_member"},
        std::pair{IdentUsage::reference_member_type,"reference_member_type"},
        std::pair{IdentUsage::maybe_type,"maybe_type"},
        std::pair{IdentUsage::reference_builtin_fn,"reference_builtin_fn"},
    };
}
constexpr void as_json(IdentUsage e,auto&& d) {
    d.value(enum_array<IdentUsage>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<IdentUsage>() {
    return "IdentUsage";
}
enum class Endian {
    unspec,
    big,
    little,
};
constexpr const char* to_string(Endian e) {
    switch(e) {
    case Endian::unspec: return "unspec";
    case Endian::big: return "big";
    case Endian::little: return "little";
    default: return nullptr;
    }
}
template<>constexpr std::optional<Endian> from_string<Endian>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "unspec") return Endian::unspec;
    if(str == "big") return Endian::big;
    if(str == "little") return Endian::little;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<Endian>() {
    return 3;
}
template<>constexpr std::array<std::pair<Endian,std::string_view>,3> make_enum_array<Endian>() {
    return {
        std::pair{Endian::unspec,"unspec"},
        std::pair{Endian::big,"big"},
        std::pair{Endian::little,"little"},
    };
}
template<>constexpr std::array<std::pair<Endian,std::string_view>,3> make_enum_name_array<Endian>() {
    return {
        std::pair{Endian::unspec,"unspec"},
        std::pair{Endian::big,"big"},
        std::pair{Endian::little,"little"},
    };
}
constexpr void as_json(Endian e,auto&& d) {
    d.value(enum_array<Endian>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<Endian>() {
    return "Endian";
}
enum class IOMethod {
    unspec,
    output_put,
    input_peek,
    input_get,
    input_backward,
    input_offset,
    input_bit_offset,
    input_remain,
    input_subrange,
    config_endian_little,
    config_endian_big,
    config_endian_native,
    config_bit_order_lsb,
    config_bit_order_msb,
};
constexpr const char* to_string(IOMethod e) {
    switch(e) {
    case IOMethod::unspec: return "unspec";
    case IOMethod::output_put: return "output_put";
    case IOMethod::input_peek: return "input_peek";
    case IOMethod::input_get: return "input_get";
    case IOMethod::input_backward: return "input_backward";
    case IOMethod::input_offset: return "input_offset";
    case IOMethod::input_bit_offset: return "input_bit_offset";
    case IOMethod::input_remain: return "input_remain";
    case IOMethod::input_subrange: return "input_subrange";
    case IOMethod::config_endian_little: return "config_endian_little";
    case IOMethod::config_endian_big: return "config_endian_big";
    case IOMethod::config_endian_native: return "config_endian_native";
    case IOMethod::config_bit_order_lsb: return "config_bit_order_lsb";
    case IOMethod::config_bit_order_msb: return "config_bit_order_msb";
    default: return nullptr;
    }
}
template<>constexpr std::optional<IOMethod> from_string<IOMethod>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "unspec") return IOMethod::unspec;
    if(str == "output_put") return IOMethod::output_put;
    if(str == "input_peek") return IOMethod::input_peek;
    if(str == "input_get") return IOMethod::input_get;
    if(str == "input_backward") return IOMethod::input_backward;
    if(str == "input_offset") return IOMethod::input_offset;
    if(str == "input_bit_offset") return IOMethod::input_bit_offset;
    if(str == "input_remain") return IOMethod::input_remain;
    if(str == "input_subrange") return IOMethod::input_subrange;
    if(str == "config_endian_little") return IOMethod::config_endian_little;
    if(str == "config_endian_big") return IOMethod::config_endian_big;
    if(str == "config_endian_native") return IOMethod::config_endian_native;
    if(str == "config_bit_order_lsb") return IOMethod::config_bit_order_lsb;
    if(str == "config_bit_order_msb") return IOMethod::config_bit_order_msb;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<IOMethod>() {
    return 14;
}
template<>constexpr std::array<std::pair<IOMethod,std::string_view>,14> make_enum_array<IOMethod>() {
    return {
        std::pair{IOMethod::unspec,"unspec"},
        std::pair{IOMethod::output_put,"output_put"},
        std::pair{IOMethod::input_peek,"input_peek"},
        std::pair{IOMethod::input_get,"input_get"},
        std::pair{IOMethod::input_backward,"input_backward"},
        std::pair{IOMethod::input_offset,"input_offset"},
        std::pair{IOMethod::input_bit_offset,"input_bit_offset"},
        std::pair{IOMethod::input_remain,"input_remain"},
        std::pair{IOMethod::input_subrange,"input_subrange"},
        std::pair{IOMethod::config_endian_little,"config_endian_little"},
        std::pair{IOMethod::config_endian_big,"config_endian_big"},
        std::pair{IOMethod::config_endian_native,"config_endian_native"},
        std::pair{IOMethod::config_bit_order_lsb,"config_bit_order_lsb"},
        std::pair{IOMethod::config_bit_order_msb,"config_bit_order_msb"},
    };
}
template<>constexpr std::array<std::pair<IOMethod,std::string_view>,14> make_enum_name_array<IOMethod>() {
    return {
        std::pair{IOMethod::unspec,"unspec"},
        std::pair{IOMethod::output_put,"output_put"},
        std::pair{IOMethod::input_peek,"input_peek"},
        std::pair{IOMethod::input_get,"input_get"},
        std::pair{IOMethod::input_backward,"input_backward"},
        std::pair{IOMethod::input_offset,"input_offset"},
        std::pair{IOMethod::input_bit_offset,"input_bit_offset"},
        std::pair{IOMethod::input_remain,"input_remain"},
        std::pair{IOMethod::input_subrange,"input_subrange"},
        std::pair{IOMethod::config_endian_little,"config_endian_little"},
        std::pair{IOMethod::config_endian_big,"config_endian_big"},
        std::pair{IOMethod::config_endian_native,"config_endian_native"},
        std::pair{IOMethod::config_bit_order_lsb,"config_bit_order_lsb"},
        std::pair{IOMethod::config_bit_order_msb,"config_bit_order_msb"},
    };
}
constexpr void as_json(IOMethod e,auto&& d) {
    d.value(enum_array<IOMethod>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<IOMethod>() {
    return "IOMethod";
}
enum class OrderType {
    byte,
    bit_stream,
    bit_mapping,
    bit_both,
};
constexpr const char* to_string(OrderType e) {
    switch(e) {
    case OrderType::byte: return "byte";
    case OrderType::bit_stream: return "bit_stream";
    case OrderType::bit_mapping: return "bit_mapping";
    case OrderType::bit_both: return "bit_both";
    default: return nullptr;
    }
}
template<>constexpr std::optional<OrderType> from_string<OrderType>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "byte") return OrderType::byte;
    if(str == "bit_stream") return OrderType::bit_stream;
    if(str == "bit_mapping") return OrderType::bit_mapping;
    if(str == "bit_both") return OrderType::bit_both;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<OrderType>() {
    return 4;
}
template<>constexpr std::array<std::pair<OrderType,std::string_view>,4> make_enum_array<OrderType>() {
    return {
        std::pair{OrderType::byte,"byte"},
        std::pair{OrderType::bit_stream,"bit_stream"},
        std::pair{OrderType::bit_mapping,"bit_mapping"},
        std::pair{OrderType::bit_both,"bit_both"},
    };
}
template<>constexpr std::array<std::pair<OrderType,std::string_view>,4> make_enum_name_array<OrderType>() {
    return {
        std::pair{OrderType::byte,"byte"},
        std::pair{OrderType::bit_stream,"bit_stream"},
        std::pair{OrderType::bit_mapping,"bit_mapping"},
        std::pair{OrderType::bit_both,"bit_both"},
    };
}
constexpr void as_json(OrderType e,auto&& d) {
    d.value(enum_array<OrderType>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<OrderType>() {
    return "OrderType";
}
enum class SpecialLiteralKind {
    input_,
    output_,
    config_,
};
constexpr const char* to_string(SpecialLiteralKind e) {
    switch(e) {
    case SpecialLiteralKind::input_: return "input";
    case SpecialLiteralKind::output_: return "output";
    case SpecialLiteralKind::config_: return "config";
    default: return nullptr;
    }
}
template<>constexpr std::optional<SpecialLiteralKind> from_string<SpecialLiteralKind>(std::string_view str) {
    if(str.empty()) return std::nullopt;
    if(str == "input") return SpecialLiteralKind::input_;
    if(str == "output") return SpecialLiteralKind::output_;
    if(str == "config") return SpecialLiteralKind::config_;
    return std::nullopt;
}
template<>constexpr size_t enum_elem_count<SpecialLiteralKind>() {
    return 3;
}
template<>constexpr std::array<std::pair<SpecialLiteralKind,std::string_view>,3> make_enum_array<SpecialLiteralKind>() {
    return {
        std::pair{SpecialLiteralKind::input_,"input"},
        std::pair{SpecialLiteralKind::output_,"output"},
        std::pair{SpecialLiteralKind::config_,"config"},
    };
}
template<>constexpr std::array<std::pair<SpecialLiteralKind,std::string_view>,3> make_enum_name_array<SpecialLiteralKind>() {
    return {
        std::pair{SpecialLiteralKind::input_,"input"},
        std::pair{SpecialLiteralKind::output_,"output"},
        std::pair{SpecialLiteralKind::config_,"config"},
    };
}
constexpr void as_json(SpecialLiteralKind e,auto&& d) {
    d.value(enum_array<SpecialLiteralKind>[int(e)].second);
}
template<>
constexpr const char* enum_type_name<SpecialLiteralKind>() {
    return "SpecialLiteralKind";
}
}
