/*license*/
#pragma once
#include <string_view>
#include <optional>
#include <set>
#include "structs.hpp"
constexpr size_t indexof(auto& s, std::string_view text) {
    size_t i = 0;
    for (auto& t : s) {
        if (t == text) {
            return i;
        }
        i++;
    }
    if (std::is_constant_evaluated()) {
        throw "invalid index";
    }
    return -1;
}

constexpr std::string_view suffixes[] = {
    // common include location
    "_before",
    "_after",
    "_pre_default",
    "_post_default",

    // visitor location
    "_pre_validate",
    "_pre_visit",
    "_post_visit",
    "_dispatch",

    // Flags location
    "_struct",
    "_bind",

    // class based
    "_class",
    // generated from DSL
    "_dsl",
};

constexpr auto suffix_before = indexof(suffixes, "_before");
constexpr auto suffix_after = indexof(suffixes, "_after");
constexpr auto suffix_pre_default = indexof(suffixes, "_pre_default");
constexpr auto suffix_post_default = indexof(suffixes, "_post_default");
constexpr auto suffix_pre_validate = indexof(suffixes, "_pre_validate");
constexpr auto suffix_pre_visit = indexof(suffixes, "_pre_visit");
constexpr auto suffix_post_visit = indexof(suffixes, "_post_visit");
constexpr auto suffix_dispatch = indexof(suffixes, "_dispatch");
constexpr auto suffix_struct = indexof(suffixes, "_struct");
constexpr auto suffix_bind = indexof(suffixes, "_bind");
constexpr auto suffix_dsl = indexof(suffixes, "_dsl");
constexpr auto suffix_class = indexof(suffixes, "_class");

constexpr std::string_view prefixes[] = {"entry", "includes", "post_includes", "pre_visitor", "pre_entry", "post_entry", "Visitor", "Flags", "Output", "Result", "Block", "Expressions", "Types", "Expression", "Type", "Statement"};

constexpr auto prefix_entry = indexof(prefixes, "entry");
constexpr auto prefix_includes = indexof(prefixes, "includes");
constexpr auto prefix_post_includes = indexof(prefixes, "post_includes");
constexpr auto prefix_pre_visitor = indexof(prefixes, "pre_visitor");
constexpr auto prefix_pre_entry = indexof(prefixes, "pre_entry");
constexpr auto prefix_post_entry = indexof(prefixes, "post_entry");
constexpr auto prefix_visitor = indexof(prefixes, "Visitor");
constexpr auto prefix_flags = indexof(prefixes, "Flags");
constexpr auto prefix_output = indexof(prefixes, "Output");
constexpr auto prefix_result = indexof(prefixes, "Result");
constexpr auto prefix_expression = indexof(prefixes, "Expression");
constexpr auto prefix_type = indexof(prefixes, "Type");
constexpr auto prefix_statement = indexof(prefixes, "Statement");

constexpr bool is_include_location(std::string_view suffix) {
    return indexof(suffixes, suffix) <= suffix_post_default;
}

constexpr bool is_visitor_location(std::string_view suffix) {
    auto loc = indexof(suffixes, suffix);
    return suffix_pre_validate <= loc && loc <= suffix_dispatch;
}

constexpr bool is_flag_location(std::string_view suffix) {
    auto loc = indexof(suffixes, suffix);
    return suffix_struct <= loc && loc <= suffix_bind;
}

struct ParsedHookName {
    std::string_view original;
    std::string_view target;
    std::string_view include_location;
    std::string_view visitor_location;
    std::string_view flags_suffix;
    std::string_view kind;
    std::optional<ebmcodegen::Struct> struct_info;
    std::set<std::string_view> body_subset;
    bool dsl = false;
    bool class_based = false;
};