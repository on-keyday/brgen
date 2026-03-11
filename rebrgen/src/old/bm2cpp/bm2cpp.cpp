/*license*/
#include "bm2cpp.hpp"
#include <code/code_writer.h>
#include <unordered_map>
#include <format>
#include <bmgen/helper.hpp>
#include <algorithm>
#include <unicode/utf/convert.h>
#include <span>
#include <escape/escape.h>
#include <bm2/context.hpp>

namespace bm2cpp {
    using TmpCodeWriter = bm2::TmpCodeWriter;

    struct Context : bm2::Context {
        Context(futils::binary::writer& w, const rebgn::BinaryModule& bm, bm2::Output& output, auto&& escape)
            : bm2::Context(w, bm, output, "r", "w", "(*this)", escape) {
        }

        std::string bytes_type = "::futils::view::rvec";
        std::string vector_type = "std::vector";
    };

    std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size = nullptr, size_t index = 0) {
        if (s.storages.size() <= index) {
            return "void";
        }
        auto& storage = s.storages[index];
        switch (storage.type) {
            case rebgn::StorageType::CODER_RETURN: {
                return "bool";
            }
            case rebgn::StorageType::PROPERTY_SETTER_RETURN: {
                return "bool";
            }
            case rebgn::StorageType::PTR: {
                auto inner = type_to_string_impl(ctx, s, bit_size, index + 1);
                return inner + "*";
            }
            case rebgn::StorageType::OPTIONAL: {
                auto inner = type_to_string_impl(ctx, s, bit_size, index + 1);
                return "std::optional<" + inner + ">";
            }
            case rebgn::StorageType::UINT: {
                auto size = storage.size().value().value();
                if (bit_size) {
                    *bit_size = size;
                }
                if (size <= 8) {
                    return "std::uint8_t";
                }
                else if (size <= 16) {
                    return "std::uint16_t";
                }
                else if (size <= 32) {
                    return "std::uint32_t";
                }
                else {
                    return "std::uint64_t";
                }
            }
            case rebgn::StorageType::INT: {
                auto size = storage.size().value().value();
                if (size <= 8) {
                    return "std::int8_t";
                }
                else if (size <= 16) {
                    return "std::int16_t";
                }
                else if (size <= 32) {
                    return "std::int32_t";
                }
                else {
                    return "std::int64_t";
                }
            }
            case rebgn::StorageType::FLOAT: {
                auto size = storage.size().value().value();
                if (size <= 32) {
                    return "float";
                }
                else {
                    return "double";
                }
            }
            case rebgn::StorageType::STRUCT_REF: {
                auto ref = storage.ref().value().value();
                auto idx = ctx.ident_index_table[ref];
                auto size = storage.size().value().value();
                if (size && bit_size) {
                    *bit_size = size - 1;
                }
                if (ctx.bm.code[idx].op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
                    return std::format("variant_{}", ref);
                }
                auto& ident = ctx.ident_table[ref];
                return ident;
            }
            case rebgn::StorageType::RECURSIVE_STRUCT_REF: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                if (ctx.ptr_type == "*") {
                    return std::format("{}*", ident);
                }
                if (ctx.ptr_type.size()) {
                    return std::format("{}<{}>", ctx.ptr_type, ident);
                }
                return std::format("std::shared_ptr<{}>", ident);
            }
            case rebgn::StorageType::ENUM: {
                auto ref = storage.ref().value().value();
                auto& ident = ctx.ident_table[ref];
                return ident;
            }
            case rebgn::StorageType::VARIANT: {
                size_t bit_size_candidate = 0;
                std::string variant = "std::variant<std::monostate";
                for (index++; index < s.storages.size(); index++) {
                    auto& storage = s.storages[index];
                    auto inner = type_to_string_impl(ctx, s, &bit_size_candidate, index);
                    variant += ", " + inner;
                    if (bit_size) {
                        *bit_size = std::max(*bit_size, bit_size_candidate);
                    }
                }
                variant += ">";
                return variant;
            }
            case rebgn::StorageType::ARRAY: {
                auto size = storage.size().value().value();
                auto inner = type_to_string_impl(ctx, s, bit_size, index + 1);
                return std::format("std::array<{}, {}>", inner, size);
            }
            case rebgn::StorageType::VECTOR: {
                auto inner = type_to_string_impl(ctx, s, bit_size, index + 1);
                if (inner == "std::uint8_t") {
                    return ctx.bytes_type;
                }
                return std::format("{}<{}>", ctx.vector_type, inner);
            }
            case rebgn::StorageType::BOOL:
                return "bool";
            default: {
                return "void";
            }
        }
    }

    std::string type_to_string(Context& ctx, rebgn::StorageRef s, size_t* bit_size = nullptr) {
        return type_to_string_impl(ctx, ctx.storage_table[s.ref.value()], bit_size);
    }

    enum class BitField {
        none,
        first,
        second,
    };

    std::string retrieve_union_type(Context& ctx, const rebgn::Code& code) {
        if (code.op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
            auto ident = code.ident().value().value();
            auto& upper = ctx.bm.code[ctx.ident_index_table[code.belong().value().value()]];
            auto base = retrieve_union_type(ctx, upper);
            return std::format("{}::variant_{}", base, ident);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_UNION ||
            code.op == rebgn::AbstractOp::DEFINE_FIELD) {
            auto belong_idx = ctx.ident_index_table[code.belong().value().value()];
            auto& belong = ctx.bm.code[belong_idx];
            return retrieve_union_type(ctx, belong);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_FORMAT) {
            auto ident = code.ident().value().value();
            return ctx.ident_table[ident];
        }
        return "";
    }

    std::string retrieve_ident_internal(Context& ctx, const rebgn::Code& code, BitField& bit_field) {
        if (code.op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
            auto ident = code.ident().value().value();
            auto& upper = ctx.bm.code[ctx.ident_index_table[code.belong().value().value()]];
            auto base = retrieve_ident_internal(ctx, upper, bit_field);
            if (bit_field != BitField::none) {
                return base;
            }
            auto type = retrieve_union_type(ctx, code);
            return std::format("std::get<{}>({})", type, base);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_UNION) {
            auto belong_idx = ctx.ident_index_table[code.belong().value().value()];
            auto& belong = ctx.bm.code[belong_idx];
            return retrieve_ident_internal(ctx, belong, bit_field);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_BIT_FIELD) {
            auto base = retrieve_ident_internal(ctx, ctx.bm.code[ctx.ident_index_table[code.belong().value().value()]], bit_field);
            bit_field = BitField::first;
            return base;
        }
        if (code.op == rebgn::AbstractOp::DEFINE_PROPERTY) {
            auto belong_idx = ctx.ident_index_table[code.belong().value().value()];
            auto base = retrieve_ident_internal(ctx, ctx.bm.code[belong_idx], bit_field);
            if (auto found = ctx.ident_table.find(code.ident().value().value());
                found != ctx.ident_table.end() && found->second.size()) {
                return std::format("(*{}.{}())", base, found->second);
            }
            return std::format("(*{}.field_{}())", base, code.ident().value().value());
        }
        if (code.op == rebgn::AbstractOp::DEFINE_FIELD) {
            auto belong_idx = ctx.ident_index_table[code.belong().value().value()];
            auto base = retrieve_ident_internal(ctx, ctx.bm.code[belong_idx], bit_field);
            std::string paren;
            if (bit_field == BitField::second) {
                return base;
            }
            else if (bit_field == BitField::first) {
                bit_field = BitField::second;
                paren = "()";
            }
            if (base.size()) {
                base += ".";
            }
            if (auto found = ctx.ident_table.find(code.ident().value().value());
                found != ctx.ident_table.end() && found->second.size()) {
                return std::format("{}{}{}", base, found->second, paren);
            }
            return std::format("{}field_{}{}", base, code.ident().value().value(), paren);
        }
        if (code.op == rebgn::AbstractOp::DEFINE_FORMAT) {
            return ctx.this_();
        }
        return "";
    }

    bool is_bit_field_property(Context& ctx, rebgn::Range range) {
        if (auto op = find_op(ctx, range, rebgn::AbstractOp::PROPERTY_FUNCTION); op) {
            // merged conditional field
            auto& p = ctx.bm.code[ctx.ident_index_table[ctx.bm.code[*op].ref()->value()]];
            if (p.merge_mode() == rebgn::MergeMode::STRICT_TYPE) {
                auto param = p.param();
                bool cont = false;
                for (auto& pa : param->refs) {
                    auto& idx = ctx.bm.code[ctx.ident_index_table[pa.value()]];
                    auto& ident = ctx.bm.code[ctx.ident_index_table[idx.right_ref()->value()]];
                    if (find_belong_op(ctx, ident, rebgn::AbstractOp::DEFINE_BIT_FIELD)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    std::string retrieve_ident(Context& ctx, const rebgn::Code& code) {
        BitField bit_field = BitField::none;
        return retrieve_ident_internal(ctx, code, bit_field);
    }

    std::vector<std::string> eval(const rebgn::Code& code, Context& ctx) {
        std::vector<std::string> res;
        switch (code.op) {
            case rebgn::AbstractOp::PHI: {
                auto ref = code.ref().value().value();
                auto idx = ctx.ident_index_table[ref];
                res = eval(ctx.bm.code[idx], ctx);
                break;
            }
            case rebgn::AbstractOp::BEGIN_COND_BLOCK: {
                auto ref = code.ref().value().value();
                auto idx = ctx.ident_index_table[ref];
                res = eval(ctx.bm.code[idx], ctx);
                break;
            }
            case rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER: {
                auto value = code.ident().value().value();
                res.push_back(std::format("param{}", value));
                break;
            }
            case rebgn::AbstractOp::ARRAY_SIZE: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                res.push_back(std::format("{}.size()", ref.back()));
                break;
            }
            case rebgn::AbstractOp::OPTIONAL_OF: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                res.push_back(ref.back());
                break;
            }
            case rebgn::AbstractOp::ADDRESS_OF: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                auto back = ref.back();
                if (back.starts_with("(*") && !back.starts_with("(*this")) {
                    back.erase(0, 2);
                    back.pop_back();
                }
                res.push_back(std::format("&{}", back));
                break;
            }
            case rebgn::AbstractOp::CAN_READ: {
                res.push_back(std::format("!r.empty()"));
                break;
            }
            case rebgn::AbstractOp::CAST: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                auto typ = code.type().value();
                auto type = type_to_string(ctx, typ);
                switch (code.cast_type().value()) {
                    case rebgn::CastType::ENUM_TO_INT:
                    case rebgn::CastType::INT_TO_ENUM:
                    case rebgn::CastType::LARGE_INT_TO_SMALL_INT:
                    case rebgn::CastType::SMALL_INT_TO_LARGE_INT:
                    case rebgn::CastType::SIGNED_TO_UNSIGNED:
                    case rebgn::CastType::UNSIGNED_TO_SIGNED: {
                        res.push_back(std::format("static_cast<{}>({})", type, ref.back()));
                        break;
                    }
                    case rebgn::CastType::INT_TO_FLOAT_BIT:
                    case rebgn::CastType::FLOAT_TO_INT_BIT: {
                        res.push_back(std::format("std::bit_cast<{}>({})", type, ref.back()));
                        break;
                    }
                    case rebgn::CastType::ARRAY_TO_VECTOR:
                    case rebgn::CastType::INT_TO_VECTOR: {
                        res.push_back(std::format("{}({})", type, ref.back()));
                        break;
                    }
                    case rebgn::CastType::VECTOR_TO_ARRAY: {
                        auto tp = ctx.storage_table[code.type()->ref.value()];
                        auto array_size = tp.storages[0].size()->value();
                        std::string res_str = type + "{";
                        for (size_t i = 0; i < array_size; i++) {
                            res_str += std::format("{}[{}],", ref.back(), i);
                        }
                        res_str.pop_back();
                        res_str += "}";
                        res.push_back(res_str);
                        break;
                    }
                    default:
                        res.push_back(std::format("/*unknown cast type {}*/", to_string(code.cast_type().value())));
                        break;
                }
                break;
            }
            case rebgn::AbstractOp::CALL_CAST: {
                auto type_str = type_to_string(ctx, *code.type());
                auto param = code.param().value();
                std::string arg_call;
                for (size_t i = 0; i < param.refs.size(); i++) {
                    auto index = ctx.ident_index_table[param.refs[i].value()];
                    auto expr = eval(ctx.bm.code[index], ctx);
                    res.insert(res.end(), expr.begin(), expr.end() - 1);
                    if (i) {
                        arg_call += ", ";
                    }
                    arg_call += expr.back();
                }
                res.push_back(std::format("({}({}))", type_str, arg_call));
                break;
            }
            case rebgn::AbstractOp::INDEX: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_index], ctx);
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                res.push_back(std::format("({})[{}]", left.back(), right.back()));
                break;
            }
            case rebgn::AbstractOp::APPEND: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_index], ctx);
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                res.push_back(std::format("{}.push_back({});", left.back(), right.back()));
                break;
            }
            case rebgn::AbstractOp::ASSIGN: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_index], ctx);
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                if (left.back().starts_with("(*") && !left.back().starts_with("(*this")) {
                    if (ctx.bm.code[right_index].op == rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER) {
                        auto back = left.back();
                        back.erase(0, 2);
                        back.pop_back();
                        res.push_back(std::format("{} = {};", back, right.back()));
                    }
                    else {
                        res.push_back(std::format("{} = {};", left.back(), right.back()));
                    }
                }
                else if (left.back().ends_with(")")) {
                    left.back().pop_back();
                    res.push_back(std::format("{}{});", left.back(), right.back()));
                }
                else {
                    res.push_back(std::format("{} = {};", left.back(), right.back()));
                }
                res.push_back(left.back());
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_TYPE: {
                res.push_back(type_to_string(ctx, *code.type()));
                break;
            }
            case rebgn::AbstractOp::ACCESS: {
                auto& left_ident = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_ident], ctx);
                auto& right_ident = ctx.ident_table[code.right_ref().value().value()];
                if (ctx.bm.code[left_ident].op == rebgn::AbstractOp::IMMEDIATE_TYPE) {
                    res.push_back(std::format("{}::{}", left.back(), right_ident));
                }
                else {
                    res.insert(res.end(), left.begin(), left.end() - 1);
                    if (find_belong_op(ctx, ctx.bm.code[ctx.ident_index_table[code.right_ref().value().value()]], rebgn::AbstractOp::DEFINE_PROPERTY)) {
                        res.push_back(std::format("{}.{}()", left.back(), right_ident));
                    }
                    else if (find_belong_op(ctx, ctx.bm.code[ctx.ident_index_table[code.right_ref().value().value()]], rebgn::AbstractOp::DEFINE_BIT_FIELD)) {
                        res.push_back(std::format("{}.{}()", left.back(), right_ident));
                    }
                    else {
                        res.push_back(std::format("{}.{}", left.back(), right_ident));
                    }
                }
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_INT64:
                res.push_back(std::format("{}", code.int_value64().value()));
                break;
            case rebgn::AbstractOp::IMMEDIATE_INT:
                res.push_back(std::format("{}", code.int_value().value().value()));
                break;
            case rebgn::AbstractOp::IMMEDIATE_CHAR: {
                std::string utf8;
                futils::utf::from_utf32(std::uint32_t(code.int_value().value().value()), utf8);
                res.push_back(std::format("{} /*{}*/", code.int_value().value().value(), utf8));
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_TRUE:
                res.push_back("true");
                break;
            case rebgn::AbstractOp::IMMEDIATE_FALSE:
                res.push_back("false");
                break;
            case rebgn::AbstractOp::IMMEDIATE_STRING: {
                auto str = ctx.string_table[code.ident().value().value()];
                res.push_back(std::format("\"{}\"", futils::escape::escape_str<std::string>(
                                                        str,
                                                        futils::escape::EscapeFlag::hex,
                                                        futils::escape::no_escape_set(),
                                                        futils::escape::escape_all())));
                break;
            }
            case rebgn::AbstractOp::BINARY: {
                auto left_index = ctx.ident_index_table[code.left_ref().value().value()];
                auto left = eval(ctx.bm.code[left_index], ctx);
                res.insert(res.end(), left.begin(), left.end() - 1);
                auto right_index = ctx.ident_index_table[code.right_ref().value().value()];
                auto right = eval(ctx.bm.code[right_index], ctx);
                res.insert(res.end(), right.begin(), right.end() - 1);
                auto bop = to_string(code.bop().value());
                res.push_back(std::format("({} {} {})", left.back(), bop, right.back()));
                break;
            }
            case rebgn::AbstractOp::UNARY: {
                auto ref_index = ctx.ident_index_table[code.ref().value().value()];
                auto ref = eval(ctx.bm.code[ref_index], ctx);
                res.insert(res.end(), ref.begin(), ref.end() - 1);
                auto uop = to_string(code.uop().value());
                res.push_back(std::format("({}{})", uop, ref.back()));
                break;
            }
            case rebgn::AbstractOp::NEW_OBJECT: {
                auto storage = *code.type();
                auto type = type_to_string(ctx, storage);
                res.push_back(std::format("{}{{}}", type));
                break;
            }
            case rebgn::AbstractOp::DEFINE_PARAMETER: {
                auto ident = ctx.ident_table[code.ident().value().value()];
                res.push_back(std::move(ident));
                break;
            }
            case rebgn::AbstractOp::DEFINE_FIELD:
            case rebgn::AbstractOp::DEFINE_PROPERTY: {
                auto range = ctx.ident_range_table[code.ident().value().value()];
                bool should_deref = false;
                if (auto type = code.type()) {
                    auto& st = ctx.storage_table[type->ref.value()];
                    if (st.storages.back().type == rebgn::StorageType::RECURSIVE_STRUCT_REF) {
                        should_deref = true;
                    }
                }
                auto ident = retrieve_ident(ctx, code);
                if (should_deref) {
                    res.push_back(std::format("(*{})", ident));
                }
                else {
                    res.push_back(std::format("{}", ident));
                }
                break;
            }
            case rebgn::AbstractOp::DEFINE_ENUM_MEMBER: {
                auto& ident = ctx.ident_table[code.ident().value().value()];
                res.push_back(ident);
                break;
            }
            case rebgn::AbstractOp::DECLARE_VARIABLE: {
                res = eval(ctx.bm.code[ctx.ident_index_table[code.ref().value().value()]], ctx);
                break;
            }
            case rebgn::AbstractOp::EMPTY_PTR: {
                res.push_back("nullptr");
                break;
            }
            case rebgn::AbstractOp::EMPTY_OPTIONAL: {
                res.push_back("std::nullopt");
                break;
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                auto& ref_index = ctx.ident_index_table[code.ref().value().value()];
                res = eval(ctx.bm.code[ref_index], ctx);
                auto& taken = res.back();
                if (auto found = ctx.ident_table.find(code.ident().value().value()); found != ctx.ident_table.end()) {
                    taken = std::format("auto {} = {};", found->second, taken);
                    res.push_back(found->second);
                }
                else {
                    taken = std::format("auto tmp{} = {};", code.ident().value().value(), taken);
                    res.push_back(std::format("tmp{}", code.ident().value().value()));
                }
                break;
            }
            case rebgn::AbstractOp::FIELD_AVAILABLE: {
                auto this_as = code.left_ref().value().value();
                if (this_as == 0) {
                    auto cond_expr = eval(ctx.bm.code[ctx.ident_index_table[code.right_ref().value().value()]], ctx);
                    res.insert(res.end(), cond_expr.begin(), cond_expr.end() - 1);
                    res.push_back(std::format("{}", cond_expr.back()));
                }
                else {
                    auto base_expr = eval(ctx.bm.code[ctx.ident_index_table[this_as]], ctx);
                    res.insert(res.end(), base_expr.begin(), base_expr.end() - 1);
                    ctx.this_as.push_back(base_expr.back());
                    auto cond_expr = eval(ctx.bm.code[ctx.ident_index_table[code.right_ref().value().value()]], ctx);
                    res.insert(res.end(), cond_expr.begin(), cond_expr.end() - 1);
                    ctx.this_as.pop_back();
                    res.push_back(cond_expr.back());
                }
                break;
            }
            default:
                res.push_back(std::format("/* Unimplemented op: {} */", to_string(code.op)));
                break;
        }
        std::vector<size_t> index;
        for (size_t i = 0; i < res.size(); i++) {
            index.push_back(i);
        }
        std::ranges::sort(index, [&](size_t a, size_t b) {
            return res[a] < res[b];
        });
        auto uniq = std::ranges::unique(index, [&](size_t a, size_t b) {
            return res[a] == res[b];
        });
        index.erase(uniq.begin(), index.end());
        std::vector<std::string> unique;
        for (size_t i = 0; i < res.size(); i++) {
            if (std::find(index.begin(), index.end(), i) == index.end()) {
                continue;
            }
            unique.push_back(std::move(res[i]));
        }
        return unique;
    }

    void code_call_parameter(Context& ctx, rebgn::Range range) {
        size_t params = 0;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            if (code.op == rebgn::AbstractOp::ENCODER_PARAMETER) {
                ctx.cw.write("w");
                params++;
            }
            if (code.op == rebgn::AbstractOp::DECODER_PARAMETER) {
                ctx.cw.write("r");
                params++;
            }
            if (code.op == rebgn::AbstractOp::STATE_VARIABLE_PARAMETER) {
                auto& ident = ctx.ident_table[code.ref().value().value()];
                if (params > 0) {
                    ctx.cw.write(", ");
                }
                ctx.cw.write(ident);
                params++;
            }
        }
    }

    void add_function_parameters(Context& ctx, rebgn::Range range, bool move_semantic = false) {
        size_t params = 0;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            if (code.op == rebgn::AbstractOp::ENCODER_PARAMETER) {
                ctx.cw.write("::futils::binary::writer& w");
                params++;
            }
            if (code.op == rebgn::AbstractOp::DECODER_PARAMETER) {
                ctx.cw.write("::futils::binary::reader& r");
                params++;
            }
            if (code.op == rebgn::AbstractOp::PROPERTY_INPUT_PARAMETER) {
                auto param_name = std::format("param{}", code.ident().value().value());
                auto type = type_to_string(ctx, *code.type());
                if (params > 0) {
                    ctx.cw.write(", ");
                }
                if (move_semantic) {
                    ctx.cw.write(type, "&& ", param_name);
                }
                else {
                    ctx.cw.write("const ", type, "& ", param_name);
                }
            }
            if (code.op == rebgn::AbstractOp::STATE_VARIABLE_PARAMETER) {
                auto storage = *ctx.bm.code[ctx.ident_index_table[code.ref()->value()]].type();
                auto type = type_to_string(ctx, storage);
                auto& ident = ctx.ident_table[code.ref().value().value()];
                if (params > 0) {
                    ctx.cw.write(", ");
                }
                ctx.cw.write(type, "& ", ident);
            }
            if (code.op == rebgn::AbstractOp::DEFINE_PARAMETER) {
                auto storage = *code.type();
                auto type = type_to_string(ctx, storage);
                auto& ident = ctx.ident_table[code.ident().value().value()];
                if (params > 0) {
                    ctx.cw.write(", ");
                }
                ctx.cw.write(type, " ", ident);
            }
        }
    }

    void inner_block(Context& ctx, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            if (code.op == rebgn::AbstractOp::DECLARE_FORMAT ||
                code.op == rebgn::AbstractOp::DECLARE_UNION ||
                code.op == rebgn::AbstractOp::DECLARE_STATE ||
                code.op == rebgn::AbstractOp::DECLARE_BIT_FIELD) {
                auto range = ctx.ident_range_table[code.ref().value().value()];
                inner_block(ctx, range);
                continue;
            }
            if (code.op == rebgn::AbstractOp::DECLARE_UNION_MEMBER) {
                if (ctx.bm_ctx.inner_bit_operations) {
                    continue;
                }
                auto range = ctx.ident_range_table[code.ref().value().value()];
                inner_block(ctx, range);
                continue;
            }
            switch (code.op) {
                case rebgn::AbstractOp::DEFINE_UNION:
                case rebgn::AbstractOp::END_UNION:
                case rebgn::AbstractOp::DECLARE_ENUM: {
                    break;
                }
                case rebgn::AbstractOp::DEFINE_UNION_MEMBER: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        break;
                    }
                    ctx.cw.writeln(std::format("struct variant_{} {{", code.ident().value().value()));
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_UNION_MEMBER: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        break;
                    }
                    defer.pop_back();
                    ctx.cw.writeln("};");
                    break;
                }
                case rebgn::AbstractOp::DECLARE_PROPERTY: {
                    auto belong = ctx.bm.code[ctx.ident_index_table[code.ref().value().value()]].belong().value().value();
                    if (belong == 0 || ctx.bm.code[ctx.ident_index_table[belong]].op == rebgn::AbstractOp::DEFINE_UNION_MEMBER) {
                        break;  // skip global property
                    }
                    auto belong_ident = ctx.ident_table[belong];
                    auto range = ctx.ident_range_table[code.ref().value().value()];
                    auto ident = ctx.ident_table[code.ref().value().value()];
                    auto getters = find_ops(ctx, range, rebgn::AbstractOp::DEFINE_PROPERTY_GETTER);
                    auto setters = find_ops(ctx, range, rebgn::AbstractOp::DEFINE_PROPERTY_SETTER);
                    for (auto& getter : getters) {
                        auto& m = ctx.bm.code[getter];
                        auto& merged = ctx.bm.code[ctx.ident_index_table[m.left_ref().value().value()]];
                        auto& func = ctx.bm.code[ctx.ident_index_table[m.right_ref().value().value()]];
                        auto mode = merged.merge_mode().value();
                        auto range = ctx.ident_range_table[func.ident().value().value()];
                        if (is_bit_field_property(ctx, range)) {
                            continue;  // skip
                        }
                        auto typ = find_op(ctx, range, rebgn::AbstractOp::RETURN_TYPE);
                        auto result = type_to_string(ctx, *ctx.bm.code[*typ].type());
                        auto getter_ident = ctx.ident_table[func.ident().value().value()];
                        ctx.cw.writeln(result, " ", getter_ident, "();");
                    }
                    for (auto& setter : setters) {
                        auto& m = ctx.bm.code[setter];
                        auto& merged = ctx.bm.code[ctx.ident_index_table[m.left_ref().value().value()]];
                        auto& func = ctx.bm.code[ctx.ident_index_table[m.right_ref().value().value()]];
                        auto mode = merged.merge_mode().value();
                        auto setter_ident = ctx.ident_table[func.ident().value().value()];
                        auto range = ctx.ident_range_table[func.ident().value().value()];
                        if (is_bit_field_property(ctx, range)) {
                            continue;  // skip
                        }
                        ctx.cw.write("bool ", setter_ident, "(");
                        add_function_parameters(ctx, range);
                        ctx.cw.writeln(");");
                    }
                    break;
                }
                case rebgn::AbstractOp::DEFINE_BIT_FIELD: {
                    ctx.cw.writeln("::futils::binary::flags_t<");
                    auto type = type_to_string(ctx, *code.type());
                    ctx.cw.write(type);
                    defer.push_back(futils::helper::defer_ex([&] {
                        ctx.cw.writeln(">");
                        auto name = ctx.bm.code[range.start].ident().value().value();
                        auto field_name = std::format("field_{}", name);
                        ctx.cw.writeln(field_name, ";");
                        ctx.bm_ctx.inner_bit_operations = false;
                        size_t counter = 0;
                        for (size_t i = range.start; i < range.end; i++) {
                            if (ctx.bm.code[i].op == rebgn::AbstractOp::DEFINE_FIELD) {
                                auto ident = ctx.ident_table[ctx.bm.code[i].ident().value().value()];
                                if (ident == "") {
                                    ident = std::format("field_{}", ctx.bm.code[i].ref().value().value());
                                }
                                auto storages = *ctx.bm.code[i].type();
                                std::string enum_ident;
                                for (auto& s : ctx.storage_table[storages.ref.value()].storages) {
                                    if (s.type == rebgn::StorageType::ENUM) {
                                        enum_ident = ctx.ident_table[s.ref().value().value()];
                                        break;
                                    }
                                }
                                if (enum_ident.size()) {
                                    ctx.cw.writeln(std::format("bits_flag_alias_method_with_enum({},{},{},{});", field_name, counter, ident, enum_ident));
                                }
                                else {
                                    ctx.cw.writeln(std::format("bits_flag_alias_method({},{},{});", field_name, counter, ident));
                                }
                                counter++;
                            }
                        }
                    }));
                    ctx.bm_ctx.inner_bit_operations = true;
                    break;
                }
                case rebgn::AbstractOp::END_BIT_FIELD: {
                    defer.pop_back();
                    break;
                }
                case rebgn::AbstractOp::DEFINE_FIELD: {
                    auto belong = code.belong().value().value();
                    if (belong == 0 || ctx.bm.code[ctx.ident_index_table[belong]].op == rebgn::AbstractOp::DEFINE_PROGRAM) {
                        break;  // skip global field
                    }
                    std::string ident;
                    if (auto found = ctx.ident_table.find(code.ident().value().value()); found != ctx.ident_table.end()) {
                        ident = found->second;
                    }
                    else {
                        ident = std::format("field_{}", code.ident().value().value());
                    }
                    auto storage = *code.type();
                    size_t bit_size = 0;
                    auto type = type_to_string(ctx, storage, &bit_size);
                    if (ctx.bm_ctx.inner_bit_operations) {
                        ctx.cw.writeln(std::format(",{} /*{}*/", bit_size, ident));
                    }
                    else {
                        ctx.cw.writeln(type, " ", ident, "{};");
                    }
                    break;
                }
                case rebgn::AbstractOp::DEFINE_FORMAT:
                case rebgn::AbstractOp::DEFINE_STATE: {
                    auto& ident = ctx.ident_table[code.ident().value().value()];
                    if (code.op == rebgn::AbstractOp::DEFINE_FORMAT) {
                        ctx.output.struct_names.push_back(ident);
                    }
                    ctx.cw.writeln("struct ", ident, " {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::DECLARE_FUNCTION: {
                    auto& ident = ctx.ident_table[code.ref().value().value()];
                    auto fn_range = ctx.ident_range_table[code.ref().value().value()];
                    auto ret = find_op(ctx, fn_range, rebgn::AbstractOp::RETURN_TYPE);
                    if (!ret) {
                        ctx.cw.write("void ", ident, "(");
                    }
                    else {
                        auto storage = *ctx.bm.code[*ret].type();
                        auto type = type_to_string(ctx, storage);
                        ctx.cw.write(type, " ", ident, "(");
                    }
                    add_function_parameters(ctx, fn_range);
                    ctx.cw.writeln(");");
                    break;
                }
                case rebgn::AbstractOp::END_FORMAT:
                case rebgn::AbstractOp::END_STATE: {
                    defer.pop_back();
                    ctx.cw.writeln("};");
                    break;
                }
                default:
                    ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    break;
            }
        }
    }

    void encode_bit_field(Context& ctx, std::uint64_t bit_size, std::uint64_t ref) {
        auto prev = ctx.bit_field_ident.back().ident;
        auto bit_counter = std::format("bit_counter{}", prev);
        auto tmp = std::format("tmp{}", prev);
        auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
        auto bit_count = bit_size;
        ctx.cw.writeln(std::format("{} <<= {};", tmp, bit_count));
        ctx.cw.writeln(std::format("{} |= {} & {};", tmp, evaluated.back(), (std::uint64_t(1) << bit_count) - 1));
        ctx.cw.writeln(std::format("{} += {};", bit_counter, bit_count));
    }

    void decode_bit_field(Context& ctx, std::uint64_t bit_size, std::uint64_t ref) {
        auto prev = ctx.bit_field_ident.back().ident;
        auto ptype = ctx.bit_field_ident.back().op;
        auto bit_counter = std::format("bit_counter{}", prev);
        auto tmp = std::format("tmp{}", prev);
        auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
        ctx.cw.writeln(std::format("{} -= {};", bit_counter, bit_size));
        if (ptype == rebgn::PackedOpType::VARIABLE) {
            auto read_size = std::format("read_size{}", prev);
            auto consumed_bits = std::format("consumed_bits{}", prev);
            auto array = std::format("tmp_array{}", prev);
            ctx.cw.writeln(std::format("{} += {};", consumed_bits, bit_size));
            ctx.cw.writeln("if (auto consumed_byte = (", consumed_bits, " + 7)/8; consumed_byte > ", read_size, ") {");
            auto scope = ctx.cw.indent_scope();
            ctx.cw.writeln(std::format("if(!r.read(::futils::view::wvec({} + {}, {} + consumed_byte))) {{ return false; }}",
                                       array, read_size, array));
            ctx.cw.writeln(std::format("for(size_t i = {};i < consumed_byte; i++) {{", read_size));
            auto scope2 = ctx.cw.indent_scope();
            ctx.cw.writeln(tmp, " |= ", array, "[i] <<", "(sizeof(", tmp, ") - i - 1);");
            scope2.execute();
            ctx.cw.writeln("}");
            scope.execute();
            ctx.cw.writeln(read_size, " = consumed_byte;");
            ctx.cw.writeln("}");
        }
        if (evaluated.back().back() == ')') {
            evaluated.back().pop_back();  // must be end with )
            ctx.cw.writeln(std::format("{}({} >> {}) & {});", evaluated.back(), tmp, bit_counter, (std::uint64_t(1) << bit_size) - 1));
        }
        else {
            ctx.cw.writeln(std::format("{} = ({} >> {}) & {};", evaluated.back(), tmp, bit_counter, (std::uint64_t(1) << bit_size) - 1));
        }
    }

    void inner_function(Context& ctx, rebgn::Range range) {
        std::vector<futils::helper::DynDefer> defer;
        for (size_t i = range.start; i < range.end; i++) {
            auto& code = ctx.bm.code[i];
            switch (code.op) {
                case rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION:
                case rebgn::AbstractOp::BEGIN_DECODE_PACKED_OPERATION: {
                    ctx.bm_ctx.inner_bit_operations = true;
                    ctx.bit_field_ident.push_back(bm2::BitFieldState{
                        .ident = code.ident()->value(),
                        .op = code.packed_op_type().value(),
                        .bit_size = code.bit_size()->value(),
                        .endian = code.endian().value(),
                    });
                    /**
                    auto bit_field = ctx.ident_range_table[code.belong().value().value()];
                    auto typ = find_op(ctx, bit_field, rebgn::AbstractOp::SPECIFY_STORAGE_TYPE);
                    if (!typ) {
                        ctx.cw.writeln("/* Unimplemented bit field *");
                        break;
                    }
                    size_t bit_size = 0;
                    auto type = type_to_string(ctx, *ctx.bm.code[*typ].type(), &bit_size);
                    auto ident = code.ident().value().value();
                    auto tmp = std::format("tmp{}", ident);
                    auto ptype = code.packed_op_type().value();
                    auto bit_counter = std::format("bit_counter{}", ident);
                    ctx.cw.writeln(type, " ", tmp, "{}; /* bit field *");
                    if (code.op == rebgn::AbstractOp::BEGIN_ENCODE_PACKED_OPERATION) {
                        ctx.cw.writeln("size_t ", bit_counter, " = 0;");

                        defer.push_back(futils::helper::defer_ex([=, &ctx]() {
                            if (ptype == rebgn::PackedOpType::FIXED) {
                                bool is_big_endian = true;  // currently only big endian is supported
                                ctx.cw.writeln("if(!::futils::binary::write_num(w, ", tmp, ", ", is_big_endian ? "true" : "false", ")) { return false; }");
                            }
                            else {
                                auto tmp_array = std::format("tmp_array{}", ident);
                                ctx.cw.writeln("unsigned char ", tmp_array, "[sizeof(", tmp, ")]{};");
                                auto byte_counter = std::format("byte_counter{}", ident);
                                ctx.cw.writeln("size_t ", byte_counter, " = (", bit_counter, " + 7) / 8;");
                                auto step = std::format("step{}", ident);
                                ctx.cw.writeln("for (size_t ", step, " = 0; ", step, " < ", byte_counter, "; ", step, "++) {");
                                auto space = ctx.cw.indent_scope();
                                ctx.cw.writeln(tmp_array, "[", step, "] = (", tmp, " >> (", byte_counter, " - ", step, " - 1) * 8) & 0xff;");
                                space.execute();
                                ctx.cw.writeln("}");
                                ctx.cw.writeln("if(!w.write(::futils::view::rvec(", tmp_array, ", ", byte_counter, "))) { return false; }");
                            }
                        }));
                    }
                    else {
                        ctx.bit_field_ident.push_back({
                            .ident = code.ident()->value(),
                            .op = ptype,
                            .bit_size = bit_size,
                            .endian = code.endian().value(),
                        });
                        ctx.cw.writeln(std::format("size_t {} = {};", bit_counter, bit_size));
                        if (ptype == rebgn::PackedOpType::FIXED) {
                            ctx.cw.writeln("if(!::futils::binary::read_num(r, ", tmp, ")) { return false; }");
                        }
                        else {
                            auto read_size = std::format("read_size{}", ident);
                            auto consumed_bits = std::format("consumed_bits{}", ident);
                            ctx.cw.writeln("size_t ", read_size, " = 0;");
                            ctx.cw.writeln("size_t ", consumed_bits, " = 0;");
                            auto array = std::format("tmp_array{}", ident);
                            ctx.cw.writeln("unsigned char ", array, "[sizeof(", tmp, ")]{};");
                        }
                    }
                    */
                    break;
                }
                case rebgn::AbstractOp::END_DECODE_PACKED_OPERATION:
                case rebgn::AbstractOp::END_ENCODE_PACKED_OPERATION: {
                    ctx.bm_ctx.inner_bit_operations = false;
                    ctx.bit_field_ident.pop_back();
                    break;
                }
                case rebgn::AbstractOp::IF: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    ctx.cw.writeln("if(", evaluated.back(), ") {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::ELIF: {
                    defer.pop_back();
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    ctx.cw.writeln("} else if(", evaluated.back(), ") {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::ELSE: {
                    defer.pop_back();
                    ctx.cw.writeln("} else {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::MATCH:
                case rebgn::AbstractOp::EXHAUSTIVE_MATCH: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    ctx.cw.writeln("switch(", evaluated.back(), ") {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::CASE: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    ctx.cw.writeln("case ", evaluated.back(), ": {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_CASE: {
                    ctx.cw.writeln("break;");
                    defer.pop_back();
                    ctx.cw.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::DEFAULT_CASE: {
                    ctx.cw.writeln("default: {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_MATCH: {
                    defer.pop_back();
                    ctx.cw.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::ASSERT: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    ctx.cw.writeln("if(!", evaluated.back(), ") { return false; }");
                    break;
                }
                case rebgn::AbstractOp::LENGTH_CHECK: {
                    auto ref_to_vec = code.left_ref().value().value();
                    auto ref_to_len = code.right_ref().value().value();
                    auto vec = eval(ctx.bm.code[ctx.ident_index_table[ref_to_vec]], ctx);
                    auto len = eval(ctx.bm.code[ctx.ident_index_table[ref_to_len]], ctx);
                    ctx.cw.writeln("if(", vec.back(), ".size() != ", len.back(), ") { return false; }");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_FUNCTION: {
                    auto fn_range = ctx.ident_range_table[code.ident().value().value()];
                    auto ret = find_op(ctx, fn_range, rebgn::AbstractOp::RETURN_TYPE);
                    if (!ret) {
                        ctx.cw.write("void ");
                    }
                    else {
                        auto storage = *ctx.bm.code[*ret].type();
                        auto type = type_to_string(ctx, storage);
                        ctx.cw.write(type, " ");
                    }
                    auto ident = ctx.ident_table[code.ident().value().value()];
                    if (auto bl = code.belong().value().value(); bl) {
                        auto typ = retrieve_union_type(ctx, ctx.bm.code[ctx.ident_index_table[bl]]);
                        ident = std::format("{}::{}", typ, ident);
                    }
                    ctx.cw.write(ident, "(");
                    add_function_parameters(ctx, fn_range);
                    ctx.cw.writeln(") {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::END_FUNCTION:
                case rebgn::AbstractOp::END_IF:
                case rebgn::AbstractOp::END_LOOP: {
                    defer.pop_back();
                    ctx.cw.writeln("}");
                    break;
                }
                case rebgn::AbstractOp::ASSIGN: {
                    auto s = eval(code, ctx);
                    ctx.cw.writeln(s[s.size() - 2]);
                    break;
                }
                case rebgn::AbstractOp::APPEND: {
                    auto s = eval(code, ctx);
                    ctx.cw.writeln(s.back());
                    break;
                }
                case rebgn::AbstractOp::INC: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    ctx.cw.writeln("++", evaluated.back(), ";");
                    break;
                }
                case rebgn::AbstractOp::CALL_ENCODE:
                case rebgn::AbstractOp::CALL_DECODE: {
                    auto ref = code.right_ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    auto name = ctx.ident_table[code.left_ref().value().value()];
                    ctx.cw.write("if(!", evaluated.back(), ".", name, "(");
                    auto range = ctx.ident_range_table[code.left_ref().value().value()];
                    code_call_parameter(ctx, range);
                    ctx.cw.writeln(")) { return false; }");
                    break;
                }
                case rebgn::AbstractOp::BREAK: {
                    ctx.cw.writeln("break;");
                    break;
                }
                case rebgn::AbstractOp::CONTINUE: {
                    ctx.cw.writeln("continue;");
                    break;
                }
                case rebgn::AbstractOp::DEFINE_VARIABLE: {
                    auto s = eval(code, ctx);
                    ctx.cw.writeln(s[s.size() - 2]);
                    break;
                }
                case rebgn::AbstractOp::DECLARE_VARIABLE: {
                    auto s = eval(code, ctx);
                    ctx.cw.writeln(s[s.size() - 2]);
                    break;
                }
                case rebgn::AbstractOp::BACKWARD_INPUT:
                case rebgn::AbstractOp::BACKWARD_OUTPUT: {
                    auto target = code.op == rebgn::AbstractOp::BACKWARD_INPUT ? "r" : "w";
                    auto ref = code.ref().value().value();
                    auto count = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    auto tmp = std::format("tmp{}", ref);
                    ctx.cw.writeln("auto ", tmp, " = ", target, ".offset();");
                    ctx.cw.writeln(target, ".reset(", tmp, " - ", count.back(), ");");
                    ctx.cw.writeln("if(", target, ".offset() != (", tmp, " - ", count.back(), ")) { return false; }");
                    break;
                }
                case rebgn::AbstractOp::ENCODE_INT: {
                    auto ref = code.ref().value().value();
                    auto& ident = ctx.ident_index_table[ref];
                    auto s = eval(ctx.bm.code[ident], ctx);
                    auto endian = code.endian().value();
                    auto is_big = endian.endian() == rebgn::Endian::little ? false : true;
                    ctx.cw.writeln("if(!::futils::binary::write_num(w,", s.back(), ",", is_big ? "true" : "false", ")) { return false; }");
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT: {
                    auto ref = code.ref().value().value();
                    auto& ident = ctx.ident_index_table[ref];
                    auto s = eval(ctx.bm.code[ident], ctx);
                    auto endian = code.endian().value();
                    auto is_big = endian.endian() == rebgn::Endian::little ? false : true;
                    ctx.cw.writeln("if(!::futils::binary::read_num(r,", s.back(), ",", is_big ? "true" : "false", ")) { return false; }");
                    break;
                }
                case rebgn::AbstractOp::ENCODE_INT_VECTOR: {
                    auto bit_size = code.bit_size().value().value();
                    auto ref_to_vec = code.left_ref().value().value();
                    auto ref_to_len = code.right_ref().value().value();
                    auto vec = eval(ctx.bm.code[ctx.ident_index_table[ref_to_vec]], ctx);
                    auto len = eval(ctx.bm.code[ctx.ident_index_table[ref_to_len]], ctx);
                    ctx.cw.writeln(std::format("if({}.size() != {}) {{ return false; }}", vec.back(), len.back()));
                    if (bit_size == 8) {
                        ctx.cw.writeln(std::format("if(!w.write({})) {{ return false; }}", vec.back()));
                    }
                    else {
                        auto tmp = std::format("i_{}", ref_to_vec);
                        ctx.cw.writeln("for(auto& ", tmp, " : ", vec.back(), ") {");
                        auto scope = ctx.cw.indent_scope();
                        ctx.cw.writeln(std::format("if(!::futils::binary::write_num(w,{},{})) {{ return false; }}", tmp, bit_size));
                        scope.execute();
                        ctx.cw.writeln("}");
                    }
                    break;
                }
                case rebgn::AbstractOp::DECODE_INT_VECTOR:
                case rebgn::AbstractOp::DECODE_INT_VECTOR_FIXED: {
                    auto bit_size = code.bit_size().value().value();
                    auto ref_to_vec = code.left_ref().value().value();
                    auto ref_to_len = code.right_ref().value().value();
                    auto vec = eval(ctx.bm.code[ctx.ident_index_table[ref_to_vec]], ctx);
                    auto len = eval(ctx.bm.code[ctx.ident_index_table[ref_to_len]], ctx);
                    if (bit_size == 8) {
                        if (code.op == rebgn::AbstractOp::DECODE_INT_VECTOR_FIXED) {
                            ctx.cw.writeln(std::format("if(!r.read(::futils::view::wvec({}).substr(0,{}))) {{ return false; }}", vec.back(), len.back()));
                        }
                        else {
                            ctx.cw.writeln(std::format("if(!r.read({},{})) {{ return false; }}", vec.back(), len.back()));
                        }
                    }
                    else {
                        auto endian = code.endian().value();
                        auto is_big = endian.endian() == rebgn::Endian::little ? false : true;
                        auto tmp_i = std::format("i_{}", ref_to_vec);
                        ctx.cw.writeln("for(size_t ", tmp_i, " = 0; ", tmp_i, " < ", len.back(), "; ", tmp_i, "++) {");
                        auto scope = ctx.cw.indent_scope();
                        if (code.op == rebgn::AbstractOp::DECODE_INT_VECTOR_FIXED) {
                            ctx.cw.writeln(std::format("if(!::futils::binary::read_num(r,{}[{}],{})) {{ return false; }}", vec.back(), tmp_i, is_big));
                        }
                        else {
                            auto tmp = std::format("tmp_hold_{}", ref_to_vec);
                            ctx.cw.writeln(std::format("std::uint{}_t {};", bit_size, tmp));
                            ctx.cw.writeln(std::format("if(!::futils::binary::read_num(r,{},{})) {{ return false; }}", tmp, is_big));
                            ctx.cw.writeln(std::format("{}.push_back({});", vec.back(), tmp));
                        }
                        scope.execute();
                        ctx.cw.writeln("}");
                    }
                    break;
                }
                case rebgn::AbstractOp::RET: {
                    auto ref = code.ref().value().value();
                    if (ref == 0) {
                        ctx.cw.writeln("return;");
                    }
                    else {
                        auto& ident = ctx.ident_index_table[ref];
                        auto s = eval(ctx.bm.code[ident], ctx);
                        ctx.cw.writeln("return ", s.back(), ";");
                    }
                    break;
                }
                case rebgn::AbstractOp::RET_SUCCESS:
                case rebgn::AbstractOp::RET_PROPERTY_SETTER_OK: {
                    ctx.cw.writeln("return true;");
                    break;
                }
                case rebgn::AbstractOp::RET_PROPERTY_SETTER_FAIL: {
                    ctx.cw.writeln("return false;");
                    break;
                }
                case rebgn::AbstractOp::LOOP_INFINITE: {
                    ctx.cw.writeln("for(;;) {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::LOOP_CONDITION: {
                    auto ref = code.ref().value().value();
                    auto evaluated = eval(ctx.bm.code[ctx.ident_index_table[ref]], ctx);
                    ctx.cw.writeln("while(", evaluated.back(), ") {");
                    defer.push_back(ctx.cw.indent_scope_ex());
                    break;
                }
                case rebgn::AbstractOp::CHECK_UNION:
                case rebgn::AbstractOp::SWITCH_UNION: {
                    if (ctx.bm_ctx.inner_bit_operations) {
                        break;
                    }
                    auto ref = code.ref().value().value();  // ref to UNION_MEMBER
                    if (find_belong_op(ctx, ctx.bm.code[ctx.ident_index_table[ref]], rebgn::AbstractOp::DEFINE_BIT_FIELD)) {
                        break;
                    }
                    auto variant_name = retrieve_union_type(ctx, ctx.bm.code[ctx.ident_index_table[ref]]);
                    // ref to DEFINE_UNION
                    auto union_belong = ctx.bm.code[ctx.ident_index_table[ref]].belong().value().value();
                    // ref to FIELD
                    auto field_ref = ctx.bm.code[ctx.ident_index_table[union_belong]].belong().value().value();
                    auto val = eval(ctx.bm.code[ctx.ident_index_table[field_ref]], ctx);
                    ctx.cw.writeln(std::format("if(!std::holds_alternative<{}>({})) {{", variant_name, val.back()));
                    auto scope = ctx.cw.indent_scope();
                    if (code.op == rebgn::AbstractOp::CHECK_UNION) {
                        auto check_at = code.check_at().value();
                        if (check_at == rebgn::UnionCheckAt::ENCODER) {
                            ctx.cw.writeln("return false;");
                        }
                        else if (check_at == rebgn::UnionCheckAt::PROPERTY_GETTER_OPTIONAL) {
                            ctx.cw.writeln("return std::nullopt;");
                        }
                        else {
                            ctx.cw.writeln("return nullptr;");
                        }
                    }
                    else {
                        ctx.cw.writeln(std::format("{} = {}{{}};", val.back(), variant_name));
                    }
                    scope.execute();
                    ctx.cw.writeln("}");
                    break;
                }
                default:
                    if (!rebgn::is_expr(code.op)) {
                        ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                    }
                    break;
            }
        }
    }

    std::string escape_cpp_keyword(const std::string& keyword) {
        if (keyword == "alignas" ||
            keyword == "alignof" ||
            keyword == "and" ||
            keyword == "and_eq" ||
            keyword == "asm" ||
            keyword == "auto" ||
            keyword == "bitand" ||
            keyword == "bitor" ||
            keyword == "bool" ||
            keyword == "break" ||
            keyword == "case" ||
            keyword == "catch" ||
            keyword == "char" ||
            keyword == "char16_t" ||
            keyword == "char32_t" ||
            keyword == "class" ||
            keyword == "compl" ||
            keyword == "const" ||
            keyword == "const_cast" ||
            keyword == "constexpr" ||
            keyword == "continue" ||
            keyword == "decltype" ||
            keyword == "default" ||
            keyword == "delete" ||
            keyword == "do" ||
            keyword == "double" ||
            keyword == "dynamic_cast" ||
            keyword == "else" ||
            keyword == "enum" ||
            keyword == "explicit" ||
            keyword == "export" ||
            keyword == "extern" ||
            keyword == "false" ||
            keyword == "final" ||
            keyword == "float" ||
            keyword == "for" ||
            keyword == "friend" ||
            keyword == "goto" ||
            keyword == "if" ||
            keyword == "inline" ||
            keyword == "int" ||
            keyword == "long" ||
            keyword == "mutable" ||
            keyword == "namespace" ||
            keyword == "new" ||
            keyword == "noexcept" ||
            keyword == "not" ||
            keyword == "not_eq" ||
            keyword == "nullptr" ||
            keyword == "operator" ||
            keyword == "or" ||
            keyword == "or_eq" ||
            keyword == "private" ||
            keyword == "protected" ||
            keyword == "public" ||
            keyword == "register" ||
            keyword == "reinterpret_cast" ||
            keyword == "requires" ||
            keyword == "return" ||
            keyword == "short" ||
            keyword == "signed" ||
            keyword == "sizeof" ||
            keyword == "static" ||
            keyword == "static_assert" ||
            keyword == "static_cast" ||
            keyword == "struct" ||
            keyword == "switch" ||
            keyword == "template" ||
            keyword == "this" ||
            keyword == "thread_local" ||
            keyword == "throw" ||
            keyword == "true" ||
            keyword == "try" ||
            keyword == "typedef" ||
            keyword == "typeid" ||
            keyword == "typename" ||
            keyword == "union" ||
            keyword == "unsigned" ||
            keyword == "using" ||
            keyword == "virtual" ||
            keyword == "void" ||
            keyword == "volatile" ||
            keyword == "wchar_t" ||
            keyword == "while" ||
            keyword == "xor" ||
            keyword == "xor_eq") {
            return std::format("{}_", keyword);
        }
        return keyword;
    }

    void to_cpp(futils::binary::writer& w, const rebgn::BinaryModule& bm, bm2::Output& output) {
        Context ctx(w, bm, output, [&](auto&& ctx, auto&& code, auto&& str) {
            if (auto range = ctx.ident_range_table.find(code); range != ctx.ident_range_table.end()) {
                if (auto f = find_op(ctx, range->second, rebgn::AbstractOp::DEFINE_FUNCTION)) {
                    if (ctx.bm.code[*f].func_type() == rebgn::FunctionType::VECTOR_SETTER) {
                        str = std::format("set_{}", str);
                    }
                }
            }
            escape_cpp_keyword(str);
        });
        bool has_union = false;
        bool has_vector = false;
        bool has_recursive = false;
        bool has_bit_field = false;
        bool has_array = false;
        bool has_optional = false;
        for (auto& typ : bm.types.maps) {
            ctx.storage_table[typ.code.value()] = typ.storage;
            for (auto& storage : typ.storage.storages) {
                if (storage.type == rebgn::StorageType::VECTOR) {
                    has_vector = true;
                    break;
                }
                if (storage.type == rebgn::StorageType::ARRAY) {
                    has_array = true;
                    break;
                }
                if (storage.type == rebgn::StorageType::RECURSIVE_STRUCT_REF) {
                    has_recursive = true;
                    break;
                }
            }
        }
        for (auto& code : bm.code) {
            if (code.op == rebgn::AbstractOp::DEFINE_UNION) {
                has_union = true;
            }
            if (code.op == rebgn::AbstractOp::DECLARE_BIT_FIELD) {
                has_bit_field = true;
            }
            if (code.op == rebgn::AbstractOp::MERGED_CONDITIONAL_FIELD &&
                code.merge_mode().value() == rebgn::MergeMode::COMMON_TYPE) {
                has_optional = true;
            }
            if (has_vector && has_union && has_recursive && has_array) {
                break;
            }
        }
        ctx.cw.writeln("// Code generated by bm2cpp of https://github.com/on-keyday/rebrgen");
        ctx.cw.writeln("#pragma once");
        ctx.cw.writeln("#include <cstdint>");
        if (has_union) {
            ctx.cw.writeln("#include <variant>");
        }
        if (has_vector) {
            ctx.cw.writeln("#include <vector>");
        }
        if (has_recursive) {
            ctx.cw.writeln("#include <memory>");
        }
        if (has_array) {
            ctx.cw.writeln("#include <array>");
        }
        if (has_optional) {
            ctx.cw.writeln("#include <optional>");
        }
        if (has_bit_field) {
            ctx.cw.writeln("#include <binary/flags.h>");
        }
        ctx.cw.writeln("#include <binary/number.h>");
        std::vector<futils::helper::DynDefer> defer;
        std::string namespace_name;
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            for (size_t i = bm.programs.ranges[j].start.value() + 1; i < bm.programs.ranges[j].end.value() - 1; i++) {
                auto& code = bm.code[i];
                switch (code.op) {
                    case rebgn::AbstractOp::METADATA: {
                        auto meta = code.metadata();
                        auto str = ctx.metadata_table[meta->name.value()];
                        if (str == "config.cpp.namespace" && meta->refs.size() == 1) {
                            if (auto found = ctx.string_table.find(meta->refs[0].value()); found != ctx.string_table.end()) {
                                namespace_name = found->second;
                            }
                        }
                        if (str == "config.cpp.include") {
                            if (auto found = ctx.string_table.find(meta->refs[0].value()); found != ctx.string_table.end()) {
                                ctx.cw.writeln("#include \"", found->second, "\"");
                            }
                        }
                        if (str == "config.cpp.sys_include") {
                            if (auto found = ctx.string_table.find(meta->refs[0].value()); found != ctx.string_table.end()) {
                                ctx.cw.writeln("#include <", found->second, ">");
                            }
                        }
                        if (str == "config.cpp.bytes_type") {
                            if (auto found = ctx.string_table.find(meta->refs[0].value()); found != ctx.string_table.end()) {
                                ctx.bytes_type = found->second;
                            }
                        }
                        if (str == "config.cpp.vector_type") {
                            if (auto found = ctx.string_table.find(meta->refs[0].value()); found != ctx.string_table.end()) {
                                ctx.vector_type = found->second;
                            }
                        }
                        break;
                    }
                }
            }
        }
        if (namespace_name.size() > 0) {
            ctx.cw.writeln("namespace ", namespace_name, " {");
            defer.push_back(ctx.cw.indent_scope_ex());
        }
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            for (size_t i = bm.programs.ranges[j].start.value() + 1; i < bm.programs.ranges[j].end.value() - 1; i++) {
                auto& code = bm.code[i];
                switch (code.op) {
                    case rebgn::AbstractOp::DECLARE_FORMAT:
                    case rebgn::AbstractOp::DECLARE_STATE: {
                        auto& ident = ctx.ident_table[code.ref().value().value()];
                        ctx.cw.writeln("struct ", ident, ";");
                        break;
                    }
                    case rebgn::AbstractOp::DECLARE_ENUM: {
                        auto& ident = ctx.ident_table[code.ref().value().value()];
                        ctx.cw.write("enum class ", ident);
                        TmpCodeWriter tmp;
                        std::string base_type;
                        auto def = ctx.ident_index_table[code.ref().value().value()];
                        if (auto ty = ctx.bm.code[def].type().value(); ty.ref.value() != 0) {
                            base_type = type_to_string(ctx, ty);
                        }
                        for (auto j = def + 1; bm.code[j].op != rebgn::AbstractOp::END_ENUM; j++) {
                            if (bm.code[j].op == rebgn::AbstractOp::DEFINE_ENUM_MEMBER) {
                                auto ident = ctx.ident_table[bm.code[j].ident().value().value()];
                                auto init = ctx.ident_index_table[bm.code[j].left_ref().value().value()];
                                auto ev = eval(bm.code[init], ctx);
                                tmp.writeln(ident, " = ", ev.back(), ",");
                            }
                        }
                        if (base_type.size() > 0) {
                            ctx.cw.write(" : ", base_type);
                        }
                        ctx.cw.writeln(" {");
                        auto d = ctx.cw.indent_scope();
                        ctx.cw.write_unformatted(tmp.out());
                        d.execute();
                        ctx.cw.writeln("};");
                        break;
                    }
                    default:
                        ctx.cw.writeln("/* Unimplemented op: ", to_string(code.op), " */");
                        break;
                }
            }
        }
        for (size_t j = 0; j < bm.programs.ranges.size(); j++) {
            inner_block(ctx, {.start = bm.programs.ranges[j].start.value() + 1, .end = bm.programs.ranges[j].end.value() - 1});
        }
        for (auto& def : ctx.on_functions) {
            def.execute();
        }
        for (size_t j = 0; j < bm.ident_ranges.ranges.size(); j++) {
            auto range = ctx.ident_range_table[bm.ident_ranges.ranges[j].ident.value()];
            if (bm.code[range.start].op != rebgn::AbstractOp::DEFINE_FUNCTION) {
                continue;
            }
            if (is_bit_field_property(ctx, range)) {
                continue;
            }
            inner_function(ctx, range);
        }
        for (auto& def : defer) {
            def.execute();
            ctx.cw.writeln("}");
        }
    }
}  // namespace bm2cpp
