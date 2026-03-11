/*license*/
#pragma once
#include <string_view>
#include <vector>
#include <map>
#include <set>
#include <ebm/extended_binary_module.hpp>
#include <code/code_writer.h>

namespace ebmcodegen {
    enum TypeAttribute {
        NONE = 0,
        ARRAY = 0x1,
        PTR = 0x2,
        RVALUE = 0x4,
    };
    struct StructField {
        std::string_view name;
        std::string_view type;
        TypeAttribute attr = NONE;
        std::string_view description;  // optional
    };

    struct Struct {
        std::string_view name;
        std::vector<StructField> fields;
        bool is_any_ref = false;
    };

    struct EnumMember {
        std::string_view name;
        size_t value = 0;
        std::string_view alt_name;
    };

    struct Enum {
        std::string_view name;
        std::vector<EnumMember> members;
    };

    std::pair<std::map<std::string_view, Struct>, std::map<std::string_view, Enum>> make_struct_map(bool include_refs = false);
    std::map<ebm::StatementKind, std::pair<std::set<std::string_view>, std::vector<std::string_view>>> body_subset_StatementBody();
    std::map<ebm::TypeKind, std::pair<std::set<std::string_view>, std::vector<std::string_view>>> body_subset_TypeBody();
    std::map<ebm::ExpressionKind, std::pair<std::set<std::string_view>, std::vector<std::string_view>>> body_subset_ExpressionBody();
    using CodeWriter = futils::code::CodeWriter<std::string>;
    std::string write_convert_from_json(const Struct& s);
    std::string write_convert_from_json(const Enum& e);
}  // namespace ebmcodegen
