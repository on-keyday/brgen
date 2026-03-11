/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include "define.hpp"
#include "generate.hpp"

namespace rebgn {

    void write_type_to_string(bm2::TmpCodeWriter& type_to_string, StorageType type, Flags& flags) {
        flags.set_func_name(bm2::FuncName::type_to_string, bm2::HookFile::type_op);
        auto type_hook = [&](auto&& default_action, bm2::HookFileSub sub = bm2::HookFileSub::main) {
            if (sub == bm2::HookFileSub::main) {
                may_write_from_hook(type_to_string, flags, bm2::HookFile::type_op, type, bm2::HookFileSub::pre_main);
            }
            if (may_write_from_hook(type_to_string, flags, bm2::HookFile::type_op, type, sub)) {
                return;
            }
            default_action();
        };
        type_to_string.writeln(std::format("case rebgn::StorageType::{}: {{", to_string(type)));
        auto scope_type = type_to_string.indent_scope();
        type_hook([&] {}, bm2::HookFileSub::before);
        if (type == StorageType::ARRAY || type == StorageType::VECTOR || type == StorageType::OPTIONAL || type == StorageType::PTR) {
            do_variable_definition(type_to_string, flags, type, "base_type", "type_to_string_impl(ctx, s, bit_size, index + 1)", "string", "base type");
        }
        if (type == StorageType::UINT || type == StorageType::INT || type == StorageType::FLOAT) {
            define_uint(type_to_string, flags, type, "size", "storage.size()->value()", "bit size");
            type_to_string.writeln("if (bit_size) {");
            auto if_block_size = type_to_string.indent_scope();
            type_to_string.writeln("*bit_size = size;");
            if_block_size.execute();
            type_to_string.writeln("}");
            if (type == StorageType::FLOAT) {
                define_uint(type_to_string, flags, type, "aligned_size", "size <= 32 ? 32 : 64", "aligned bit size");
            }
            else {
                define_uint(type_to_string, flags, type, "aligned_size", "size <= 8 ? 8 : size <= 16 ? 16 : size <= 32 ? 32 : 64", "aligned bit size");
            }

            type_hook([&] {
                std::map<std::string, std::string> map{
                    {"BIT_SIZE", "\",futils::number::to_string<std::string>(size),\""},
                    {"ALIGNED_BIT_SIZE", "\",futils::number::to_string<std::string>(aligned_size),\""},
                };
                if (type == StorageType::FLOAT) {
                    auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(float_type), map);
                    type_to_string.writeln("return ", escaped, ";");
                }
                else if (type == StorageType::INT) {
                    auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(int_type), map);
                    type_to_string.writeln("return ", escaped, ";");
                }
                else {
                    auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(uint_type), map);
                    type_to_string.writeln("return ", escaped, ";");
                }
            });
        }
        else if (type == StorageType::STRUCT_REF) {
            define_ident(type_to_string, flags, type, "ident", "storage.ref().value()", "struct");
            type_hook([&] {
                type_to_string.writeln("return ident;");
            });
        }
        else if (type == StorageType::RECURSIVE_STRUCT_REF) {
            define_ident(type_to_string, flags, type, "ident", "storage.ref().value()", "recursive struct");
            type_hook([&] {
                std::map<std::string, std::string> map{
                    {"TYPE", "\",ident,\""},
                };
                auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(recursive_struct_type), map);
                type_to_string.writeln("return ", escaped, ";");
            });
        }
        else if (type == StorageType::BOOL) {
            type_hook([&] {
                type_to_string.writeln("return \"", USE_FLAG_BASE(type, bool_type), "\";");
            });
        }
        else if (type == StorageType::ENUM) {
            define_ident(type_to_string, flags, type, "ident", "storage.ref().value()", "enum");
            type_hook([&] {
                type_to_string.writeln("return ident;");
            });
        }
        else if (type == StorageType::VARIANT) {
            define_ident(type_to_string, flags, type, "ident", "storage.ref().value()", "variant");
            do_typed_variable_definition(type_to_string, flags, type, "types", "{}", "std::vector<std::string>", "variant types");
            do_typed_variable_definition(type_to_string, flags, type, "variant_size", "0", "std::optional<size_t>", "variant storage size");
            type_to_string.writeln("for (size_t i = index + 1; i < s.storages.size(); i++) {");
            auto scope_variant = type_to_string.indent_scope();
            {
                type_to_string.writeln("types.push_back(type_to_string_impl(ctx, s, bit_size, i));");
                type_to_string.writeln("auto candidate = s.storages[i].size()->value();");
                type_to_string.writeln("if (candidate == 0) {");
                auto if_block_variant = type_to_string.indent_scope();
                type_to_string.writeln("variant_size = std::nullopt;");
                if_block_variant.execute();
                type_to_string.writeln("}");
                type_to_string.writeln("else if (variant_size&&variant_size < (candidate - 1)) {");
                auto elif_block_variant = type_to_string.indent_scope();
                type_to_string.writeln("variant_size = candidate - 1;");
                elif_block_variant.execute();
                type_to_string.writeln("}");
            }
            scope_variant.execute();
            type_to_string.writeln("}");
            type_to_string.writeln("if (variant_size&&bit_size) {");
            auto if_block_variant_size = type_to_string.indent_scope();
            type_to_string.writeln("*bit_size = *variant_size;");
            if_block_variant_size.execute();
            type_to_string.writeln("}");
            type_hook([&] {
                if (USE_FLAG_BASE(type, variant_mode) == "union") {
                    type_to_string.writeln("return ident;");
                }
                else if (USE_FLAG_BASE(type, variant_mode) == "algebraic") {
                    type_to_string.writeln("std::string result;");
                    type_to_string.writeln("for (size_t i = 0; i < types.size(); i++) {");
                    auto scope_variant_algebraic = type_to_string.indent_scope();
                    type_to_string.writeln("if (i != 0) {");
                    auto if_block_variant_algebraic = type_to_string.indent_scope();
                    type_to_string.writeln("result += \" ", USE_FLAG_BASE(type, algebraic_variant_separator), " \";");
                    if_block_variant_algebraic.execute();
                    type_to_string.writeln("}");
                    type_to_string.writeln("result += types[i];");
                    scope_variant_algebraic.execute();
                    type_to_string.writeln("}");
                    std::map<std::string, std::string> map{
                        {"VARIANT", "\",ident,\""},
                        {"TYPES", "\",result,\""},
                    };
                    auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(algebraic_variant_type), map);
                    type_to_string.writeln("return ", escaped, ";");
                }
            });
        }
        else if (type == StorageType::CODER_RETURN) {
            type_hook([&] {
                type_to_string.writeln("return \"", USE_FLAG_BASE(type, coder_return_type), "\";");
            });
        }
        else if (type == StorageType::PROPERTY_SETTER_RETURN) {
            type_hook([&] {
                type_to_string.writeln("return \"", USE_FLAG_BASE(type, property_setter_return_type), "\";");
            });
        }
        else if (type == StorageType::PTR) {
            type_hook([&] {
                std::map<std::string, std::string> map{
                    {"TYPE", "\",base_type,\""},
                };
                auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(pointer_type), map);
                type_to_string.writeln("return ", escaped, ";");
            });
        }
        else if (type == StorageType::ARRAY) {
            define_bool(type_to_string, flags, type, "is_byte_vector", "index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8", "is byte vector");
            define_uint(type_to_string, flags, type, "length", "storage.size()->value()", "array length");
            type_hook([&] {
                std::map<std::string, std::string> map{
                    {"TYPE", "\",base_type,\""},
                    {"LEN", "\",futils::number::to_string<std::string>(length),\""},
                };
                auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(array_type), map);
                if (USE_FLAG_BASE(type, byte_array_type).size()) {
                    type_to_string.writeln("if (is_byte_vector) {");
                    auto if_block_byte_vector = type_to_string.indent_scope();
                    auto escaped2 = env_escape_and_concat(flags, type, ENV_FLAG(byte_array_type), map);
                    type_to_string.writeln("return ", escaped2, ";");
                    if_block_byte_vector.execute();
                    type_to_string.writeln("}");
                }
                type_to_string.writeln("return ", escaped, ";");
            });
        }
        else if (type == StorageType::VECTOR) {
            define_bool(type_to_string, flags, type, "is_byte_vector", "index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8", "is byte vector");
            type_hook([&] {
                if (USE_FLAG_BASE(type, byte_vector_type).size()) {
                    type_to_string.writeln("if (is_byte_vector) {");
                    auto if_block_byte_vector = type_to_string.indent_scope();
                    type_to_string.writeln("return \"", USE_FLAG_BASE(type, byte_vector_type), "\";");
                    if_block_byte_vector.execute();
                    type_to_string.writeln("}");
                }
                std::map<std::string, std::string> map{
                    {"TYPE", "\",base_type,\""},
                };
                auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(vector_type), map);
                type_to_string.writeln("return ", escaped, ";");
            });
        }
        else if (type == StorageType::OPTIONAL) {
            type_hook([&] {
                std::map<std::string, std::string> map{
                    {"TYPE", "\",base_type,\""},
                };
                auto escaped = env_escape_and_concat(flags, type, ENV_FLAG(optional_type), map);
                type_to_string.writeln("return ", escaped, ";");
            });
        }
        type_hook([&] {}, bm2::HookFileSub::after);
        scope_type.execute();
        type_to_string.writeln("}");
    }

    void write_type_to_string_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& type_to_string, Flags& flags) {
        w.writeln("std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size = nullptr, size_t index = 0);");
        type_to_string.writeln("std::string type_to_string_impl(Context& ctx, const rebgn::Storages& s, size_t* bit_size, size_t index) {");
        auto scope_type_to_string = type_to_string.indent_scope();
        type_to_string.writeln("if (s.storages.size() <= index) {");
        auto if_block_type = type_to_string.indent_scope();
        type_to_string.writeln("return \"", flags.wrap_comment("type index overflow"), "\";");
        if_block_type.execute();
        type_to_string.writeln("}");
        type_to_string.writeln("auto& storage = s.storages[index];");
        type_to_string.writeln("switch (storage.type) {");

        auto switch_scope = type_to_string.indent_scope();
        for (size_t i = 0; to_string(StorageType(i))[0] != 0; i++) {
            auto type = StorageType(i);
            write_type_to_string(type_to_string, type, flags);
        }
        type_to_string.writeln("default: {");
        auto if_block_type_default = type_to_string.indent_scope();
        type_to_string.writeln("return ", unimplemented_comment(flags, "storage.type"), ";");
        if_block_type_default.execute();
        type_to_string.writeln("}");
        switch_scope.execute();
        type_to_string.writeln("}");
        scope_type_to_string.execute();
        type_to_string.writeln("}");

        w.writeln("std::string type_to_string(Context& ctx, const rebgn::StorageRef& ref,size_t* bit_size = nullptr);");
        type_to_string.writeln("std::string type_to_string(Context& ctx, const rebgn::StorageRef& ref,size_t* bit_size) {");
        auto scope_type_to_string_ref = type_to_string.indent_scope();
        type_to_string.writeln("auto& storage = ctx.storage_table[ref.ref.value()];");
        type_to_string.writeln("return type_to_string_impl(ctx, storage, bit_size);");
        scope_type_to_string_ref.execute();
        type_to_string.writeln("}");
    }

}  // namespace rebgn