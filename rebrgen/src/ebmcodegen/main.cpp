/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "../ebm/extended_binary_module.hpp"
#include <helper/template_instance.h>
#include <strutil/append.h>
#include <file/file_stream.h>  // Required for futils::file::FileStream
#include <binary/writer.h>     // Required for futils::binary::writer
#include <file/file_view.h>
#include <code/code_writer.h>
#include <ebmgen/mapping.hpp>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include "error/error.h"
#include "helper/expected.h"
#include "json/stringer.h"
#include "stub/structs.hpp"
#include "dsl/dsl.h"
#include "stub/url.hpp"
#include "stub/hooks.hpp"
#include "stub/class_based.hpp"

enum class GenerateMode {
    Template,
    BodySubset,
    JSONConverterHeader,
    JSONConverterSource,
    CMake,
    CodeGenerator,
    ClassBasedCodeGeneratorHeader,
    ClassBasedCodeGeneratorSource,
    Interpreter,
    ClassBasedInterpreterHeader,
    ClassBasedInterpreterSource,
    EbmgenVisitor,
    Accessor,
    HookList,
    HookKind,
    SpecJSON,
    DSL,
};

struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view lang = "cpp";
    std::string_view program_name;
    GenerateMode mode = GenerateMode::CodeGenerator;
    std::string_view visitor_impl_dir = "visitor/";
    std::string_view visitor_impl_dsl_dir = "visitor/dsl/";
    std::string_view default_visitor_impl_dir = "ebmcodegen/default_codegen_visitor/";
    std::string_view template_target;
    std::string_view dsl_file;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&lang, "l,lang", "language for output", "LANG");
        ctx.VarString<true>(&program_name, "p,program-name", "program name (default: ebm2{lang})", "PROGRAM_NAME");
        ctx.VarString<true>(&visitor_impl_dir, "d,visitor-impl-dir", "directory for visitor implementation", "DIR");
        ctx.VarString<true>(&visitor_impl_dsl_dir, "visitor-impl-dsl-dir", "directory for visitor implementation that is generated from DSL", "DIR");
        ctx.VarString<true>(&default_visitor_impl_dir, "default-visitor-impl-dir", "directory for default visitor implementation", "DIR");
        ctx.VarString<true>(&template_target, "template-target", "template target name. see --mode hooklist", "target_name");
        ctx.VarString<true>(&dsl_file, "dsl-file", "DSL source file for --mode dsl", "FILE");
        ctx.VarMap(&mode, "mode", "generate mode (default: codegen)", "{subset,codegen,interpret,hooklist,hookkind,template,spec-json,dsl,accessor,ebmgen-visitor,codegen-class-header,codegen-class-source,interpret-class-header,interpret-class-source,json-conv-header,json-conv-source}",
                   std::map<std::string, GenerateMode>{
                       {"template", GenerateMode::Template},
                       {"subset", GenerateMode::BodySubset},
                       {"json-conv-header", GenerateMode::JSONConverterHeader},
                       {"json-conv-source", GenerateMode::JSONConverterSource},
                       {"cmake", GenerateMode::CMake},
                       {"codegen", GenerateMode::CodeGenerator},
                       {"codegen-class-header", GenerateMode::ClassBasedCodeGeneratorHeader},
                       {"codegen-class-source", GenerateMode::ClassBasedCodeGeneratorSource},
                       {"interpret", GenerateMode::Interpreter},
                       {"interpret-class-header", GenerateMode::ClassBasedInterpreterHeader},
                       {"interpret-class-source", GenerateMode::ClassBasedInterpreterSource},
                       {"ebmgen-visitor", GenerateMode::EbmgenVisitor},
                       {"hooklist", GenerateMode::HookList},
                       {"hookkind", GenerateMode::HookKind},
                       {"spec-json", GenerateMode::SpecJSON},
                       {"dsl", GenerateMode::DSL},
                       {"accessor", GenerateMode::Accessor},
                   });
    }
};

auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();

namespace ebmgen {
    bool verbose_error = false;
}

namespace ebmcodegen {
    ebmgen::expected<void> class_based_hook_descriptions(CodeWriter& w, const ParsedHookName& hook_name, const std::map<std::string_view, Struct>& structs);
    std::vector<std::string> class_based_hook_names(const std::map<std::string_view, Struct>& structs, const IncludeLocations& locations);
    void generate_constexpr_access(CodeWriter& w, const std::map<std::string_view, Struct>& struct_map, const std::map<std::string_view, Enum>& enum_map);
}  // namespace ebmcodegen

auto error(auto& fmt, auto&&... arg) {
    return futils::helper::either::unexpected{futils::error::StrError<std::string>(std::vformat(fmt, std::make_format_args(std::forward<decltype(arg)>(arg)...)))};
}

ebmgen::expected<ParsedHookName> parse_hook_name(std::string_view parsed, const std::map<std::string_view, ebmcodegen::Struct>& structs) {
    ParsedHookName result;
    result.original = parsed;
    for (auto& prefix : prefixes) {
        if (parsed.starts_with(prefix)) {
            parsed = parsed.substr(prefix.size());
            result.target = prefix;
            break;
        }
    }
    if (result.target.empty()) {
        return error("Empty target prefix; got parsed: {} {} {} {}", parsed, result.include_location, result.visitor_location, result.flags_suffix);
    }
    bool found_suffix = true;
    while (found_suffix) {
        found_suffix = false;
        for (auto& suffix : suffixes) {
            if (parsed.ends_with(suffix)) {
                parsed = parsed.substr(0, parsed.size() - suffix.size());
                if (is_include_location(suffix)) {
                    if (result.include_location.size()) {
                        return error("Duplicate include location suffix: {} vs {}", result.include_location, suffix);
                    }
                    result.include_location = suffix;
                }
                else if (is_visitor_location(suffix)) {
                    if (result.visitor_location.size()) {
                        return error("Duplicate visitor location suffix: {} vs {}", result.visitor_location, suffix);
                    }
                    result.visitor_location = suffix;
                }
                else if (is_flag_location(suffix)) {
                    if (result.flags_suffix.size()) {
                        return error("Duplicate flags suffix: {} vs {}", result.flags_suffix, suffix);
                    }
                    result.flags_suffix = suffix;
                }
                else if (suffix == suffixes[suffix_dsl]) {
                    result.dsl = true;
                }
                else if (suffix == suffixes[suffix_class]) {
                    result.class_based = true;
                }
                else {
                    return error("Unknown suffix: {}", suffix);
                }
                found_suffix = true;
                break;
            }
        }
    }
    std::set<std::string_view> sub_existence;
    if (parsed.starts_with("_")) {
        parsed = parsed.substr(1);
        if (result.target == "Expression") {
            auto expr = ebm::ExpressionKind_from_string(parsed);
            if (!expr) {
                return error("Invalid expression: {}", parsed);
            }
            result.kind = parsed;
            sub_existence = ebmcodegen::body_subset_ExpressionBody()[*expr].first;
            result.struct_info = structs.find("ExpressionBody")->second;
        }
        else if (result.target == "Type") {
            auto type = ebm::TypeKind_from_string(parsed);
            if (!type) {
                return error("Invalid type: {}", parsed);
            }
            result.kind = parsed;
            sub_existence = ebmcodegen::body_subset_TypeBody()[*type].first;
            result.struct_info = structs.find("TypeBody")->second;
        }
        else if (result.target == "Statement") {
            auto stmt = ebm::StatementKind_from_string(parsed);
            if (!stmt) {
                return error("Invalid statement: {}", parsed);
            }
            result.kind = parsed;
            sub_existence = ebmcodegen::body_subset_StatementBody()[*stmt].first;
            result.struct_info = structs.find("StatementBody")->second;
        }
        else {
            return error("Unknown target: {}", result.target);
        }
    }
    else if (parsed.size()) {
        return error("unexpected remaining element: {}", parsed);
    }
    result.body_subset = std::move(sub_existence);
    return result;
}

using CodeWriter = ebmcodegen::CodeWriter;

int print_cmake(CodeWriter& w, Flags& flags) {
    auto target_name = flags.program_name;
    w.writeln("#license");
    w.writeln("# Code generated by ebmcodegen at ", repo_url);
    w.writeln("project(", target_name, ")");
    w.writeln("set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tool)");
    w.writeln("add_executable(", target_name);
    w.indent_writeln("\"main.cpp\"");
    w.writeln(")");
    w.write("target_precompile_headers(", target_name, " ");
    // codegen.hpp includes user generated code so exclude from this
    w.write_unformatted(R"(PRIVATE
    <format>
    <expected>
    <string>
    <vector>
    <variant>
    <optional>
    <ebm/extended_binary_module.hpp>
    <ebmcodegen/stub/entry.hpp>
    <ebmcodegen/stub/util.hpp>
    <ebmgen/common.hpp>
    <ebmgen/convert/helper.hpp>
    <ebmgen/mapping.hpp>
    <code/code_writer.h>
    <code/loc_writer.h>
    <ebmcodegen/stub/writer_manager.hpp>
    <concepts>
    <strutil/append.h>
)
)");
    w.writeln("if(UNIX)");
    w.writeln("set_target_properties(", target_name, " PROPERTIES INSTALL_RPATH \"${CMAKE_SOURCE_DIR}/tool\")");
    w.writeln("endif()");
    w.writeln("target_link_libraries(", target_name, " ebm futils ebm_mapping)");
    w.writeln("install(TARGETS ", target_name, " DESTINATION tool)");
    w.writeln("if (\"$ENV{BUILD_MODE}\" STREQUAL \"web\")");
    w.indent_writeln("target_compile_options(", target_name, " PUBLIC \"-gsource-map\")");
    w.indent_writeln("target_link_options(", target_name, " PUBLIC \"-gsource-map\" \"--source-map-base\" \"http://localhost:8000/\")");
    w.indent_writeln("install(FILES \"${CMAKE_BINARY_DIR}/tool/", target_name, ".wasm\" DESTINATION tool)");
    w.indent_writeln("install(FILES \"${CMAKE_BINARY_DIR}/tool/", target_name, ".wasm.map\" DESTINATION tool)");
    w.writeln("endif()");
    cout << w.out();
    return 0;
}

int print_spec_json(std::map<std::string_view, ebmcodegen::Struct>& struct_map, std::map<std::string_view, ebmcodegen::Enum>& enum_map) {
    using Stringer = futils::json::Stringer<>;
    Stringer stringer;
    {
        auto root = stringer.object();
        root("structs", [&](Stringer& s) {
            auto element = s.array();
            for (auto& object : struct_map) {
                element([&](Stringer& js) {
                    auto obj = js.object();
                    obj("name", object.second.name);
                    obj("fields", [&](Stringer& s) {
                        auto element = s.array();
                        if (object.second.name == "ExtendedBinaryModule") {  // add magic as optional field
                            element([&](Stringer& js) {
                                auto object = js.object();
                                object("name", "magic");
                                object("type", "std::string");
                                object("is_array", false);
                                object("is_pointer", false);
                            });
                        }
                        for (auto& s : object.second.fields) {
                            element([&](Stringer& js) {
                                auto object = js.object();
                                object("name", s.name);
                                object("type", s.type);
                                object("is_array", bool(s.attr & ebmcodegen::ARRAY));
                                object("is_pointer", bool(s.attr & ebmcodegen::PTR));
                            });
                        }
                    });
                });
            }
        });
        root("enums", [&](Stringer& s) {
            auto element = s.array();
            for (auto& object : enum_map) {
                element([&](Stringer& js) {
                    auto obj = js.object();
                    obj("name", object.second.name);
                    obj("members", [&](Stringer& s) {
                        auto element = s.array();
                        for (auto& s : object.second.members) {
                            element([&](Stringer& js) {
                                auto object = js.object();
                                object("name", s.name);
                                if (s.alt_name.size()) {
                                    object("alt_name", s.alt_name);
                                }
                                object("value", s.value);
                            });
                        }
                    });
                });
            }
        });
        root("subset_info", [&] {
            auto subset_info = [&](std::string_view name, auto t, auto info) {
                using T = decltype(t);
                auto object = stringer.object();
                object("name", name);
                object("available_field_per_kind", [&] {
                    auto element = stringer.array();
                    for (auto& s : info) {
                        element([&] {
                            auto object = stringer.object();
                            object("kind", to_string(s.first, true));
                            object("fields", s.second.second);
                        });
                    }
                });
            };
            auto element = stringer.array();
            element([&] {
                subset_info("TypeBody", ebm::TypeKind{}, ebmcodegen::body_subset_TypeBody());
            });
            element([&] {
                subset_info("ExpressionBody", ebm::TypeKind{}, ebmcodegen::body_subset_ExpressionBody());
            });
            element([&] {
                subset_info("StatementBody", ebm::TypeKind{}, ebmcodegen::body_subset_StatementBody());
            });
        });
    }
    cout << stringer.out();
    return 0;
}

int print_json_converter(CodeWriter& w, bool src) {
    auto [struct_map, enum_map] = ebmcodegen::make_struct_map(true);
    w.writeln("#include <ebm/extended_binary_module.hpp>");
    if (src) {
        w.writeln("#include <json/convert_json.h>");
    }
    w.writeln("#include <json/json.h>");
    w.writeln("namespace ebm {");
    {
        auto scope = w.indent_scope();
        for (auto& s : struct_map) {
            if (src) {
                w.write_unformatted(ebmcodegen::write_convert_from_json(s.second));
            }
            else {
                w.writeln("bool from_json(", s.second.name, "& obj, const futils::json::JSON& j);");
            }
            w.writeln();
        }
        for (auto& e : enum_map) {
            if (src) {
                w.write_unformatted(ebmcodegen::write_convert_from_json(e.second));
            }
            else {
                w.writeln("bool from_json(", e.second.name, "& obj, const futils::json::JSON& j);");
            }
            w.writeln();
        }
    }
    w.writeln("} // namespace ebm");
    cout << w.out();
    return 0;
}

int print_body_subset(CodeWriter& w, std::map<std::string_view, ebmcodegen::Struct>& struct_map) {
    w.writeln("#include <ebm/extended_binary_module.hpp>");
    w.writeln("#include <set>");
    w.writeln("#include <map>");
    w.writeln("namespace ebmcodegen {");
    {
        auto scope = w.indent_scope();
        auto do_visit_body = [&](auto t, ebmcodegen::Struct& s) {
            using T = std::decay_t<decltype(t)>;
            auto map_type = std::format("std::map<ebm::{},std::pair<std::set<std::string_view>,std::vector<std::string_view>>>", visit_enum(t));
            w.writeln(map_type, " body_subset_", s.name, "() {");
            auto scope2 = w.indent_scope();
            w.writeln(map_type, " subset_map;");
            w.writeln("for(size_t i = 0;to_string(ebm::", visit_enum(t), "(i))[0];i++) {");
            auto _scope = w.indent_scope();
            w.writeln("ebm::", s.name, " body;");
            w.writeln("body.kind = ebm::", visit_enum(t), "(i);");
            for (auto& f : s.fields) {
                if (f.attr & ebmcodegen::TypeAttribute::PTR) {
                    w.writeln("body.", f.name, "({});");
                }
            }
            w.writeln("std::set<std::string_view> subset;");
            w.writeln("std::vector<std::string_view> ordered;");
            w.writeln("body.visit([&](auto&& visitor,const char* name,auto&& value) {");
            auto _scope2 = w.indent_scope();
            w.writeln("using T = std::decay_t<decltype(value)>;");
            w.writeln("if constexpr (std::is_pointer_v<T>) {");
            w.indent_writeln("if (value) { subset.insert(name); ordered.push_back(name); }");
            w.writeln("}");
            w.writeln("else {");
            w.indent_writeln("subset.insert(name); ordered.push_back(name);");
            w.writeln("}");
            _scope2.execute();
            w.writeln("});");
            w.writeln("subset_map[ebm::", visit_enum(t), "(i)] = std::make_pair(std::move(subset), std::move(ordered));");
            _scope.execute();
            w.writeln("}");
            w.writeln("return subset_map;");
            scope2.execute();
            w.writeln("}");
        };

        do_visit_body(ebm::StatementKind{}, struct_map["StatementBody"]);
        do_visit_body(ebm::ExpressionKind{}, struct_map["ExpressionBody"]);
        do_visit_body(ebm::TypeKind{}, struct_map["TypeBody"]);
    }
    w.writeln("} // namespace ebmcodegen");
    cout << w.out();
    return 0;
}

int print_hook_kind() {
    futils::json::Stringer<> stringer;
    {
        auto root = stringer.object();
        root("prefixes", [&](futils::json::Stringer<>& s) {
            auto element = s.array();
            for (auto& p : prefixes) {
                element(p);
            }
        });
        root("suffixes", [&](futils::json::Stringer<>& s) {
            auto element = s.array();
            for (auto& p : suffixes) {
                element(p);
            }
        });
    }
    cout << stringer.out();
    return 0;
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (flags.mode == GenerateMode::DSL) {
        if (flags.dsl_file.empty()) {
            cerr << "dsl-file is required for dsl mode\n";
            return 1;
        }
        futils::file::View view;
        if (auto res = view.open(flags.dsl_file); !res) {
            cerr << "Failed to open DSL file: " << res.error().error<std::string>() << "\n";
            return 1;
        }
        auto dsl_source = std::string_view((const char*)view.data(), view.size());
        auto res = ebmcodegen::dsl::generate_dsl_output(flags.dsl_file, dsl_source);
        if (!res) {
            cerr << "Failed to generate DSL output: " << res.error().error() << "\n";
            return 1;
        }
        cout << *res;
        return 0;
    }
    std::string prog_name_buf;
    if (flags.program_name.empty()) {
        prog_name_buf = std::format("ebm2{}", flags.lang);
        flags.program_name = prog_name_buf;
    }
    CodeWriter w;
    if (flags.mode == GenerateMode::CMake) {
        return print_cmake(w, flags);
    }
    if (flags.mode == GenerateMode::HookKind) {
        return print_hook_kind();
    }
    auto write_header = [&] {
        w.writeln("/*license*/");
        w.writeln("// Code generated by ebmcodegen at ", repo_url);
    };
    if (flags.mode == GenerateMode::JSONConverterSource || flags.mode == GenerateMode::JSONConverterHeader) {
        write_header();
        return print_json_converter(w, flags.mode == GenerateMode::JSONConverterSource);
    }
    auto [struct_map, enum_map] = ebmcodegen::make_struct_map();
    if (flags.mode == GenerateMode::SpecJSON) {
        return print_spec_json(struct_map, enum_map);
    }
    write_header();
    if (flags.mode == GenerateMode::BodySubset) {
        return print_body_subset(w, struct_map);
    }
    if (flags.mode == GenerateMode::ClassBasedCodeGeneratorHeader || flags.mode == GenerateMode::ClassBasedInterpreterHeader) {
        w.writeln("#pragma once");
    }
    if (flags.mode == GenerateMode::Accessor) {
        ebmcodegen::generate_constexpr_access(w, struct_map, enum_map);
        cout << w.out();
        return 0;
    }
    w.writeln("// DO NOT EDIT THIS FILE MANUALLY. you should edit visitor implementation files instead.");
    if (flags.mode != GenerateMode::EbmgenVisitor) {  // if ebmgen mode, no entry point needed
        w.writeln("#include <ebmcodegen/stub/entry.hpp>");
        w.writeln("#include <ebmcodegen/stub/util.hpp>");
    }
    w.writeln("#include <ebmcodegen/stub/context.hpp>");
    w.writeln("#include <ebmcodegen/stub/visitor.hpp>");
    w.writeln("#include <ebmgen/common.hpp>");
    w.writeln("#include <ebmgen/convert/helper.hpp>");
    w.writeln("#include <ebmgen/mapping.hpp>");
    w.writeln("#include <code/code_writer.h>");
    w.writeln("#include <ebmcodegen/stub/code_writer.hpp>");
    w.writeln("#include <ebmcodegen/stub/writer_manager.hpp>");

    auto ns_name = flags.program_name;
    ebmcodegen::IncludeLocations locations;
    locations.include_locations = {
        {flags.visitor_impl_dir, std::string{suffixes[suffix_class]}},
        {flags.visitor_impl_dir, std::string{}},

        {flags.visitor_impl_dsl_dir, std::string{suffixes[suffix_dsl]} + std::string(suffixes[suffix_class])},
        {flags.visitor_impl_dsl_dir, std::string{suffixes[suffix_dsl]}},

        {flags.default_visitor_impl_dir, std::string{suffixes[suffix_class]}},
        {flags.default_visitor_impl_dir, std::string{}},
    };
    locations.ns_name = ns_name;
    locations.lang = flags.lang;
    locations.program_name = flags.program_name;
    locations.is_codegen = (flags.mode == GenerateMode::ClassBasedCodeGeneratorHeader || flags.mode == GenerateMode::ClassBasedCodeGeneratorSource);
    locations.ebmgen_mode = (flags.mode == GenerateMode::EbmgenVisitor);

    if (flags.mode == GenerateMode::ClassBasedCodeGeneratorHeader || flags.mode == GenerateMode::ClassBasedCodeGeneratorSource ||
        flags.mode == GenerateMode::ClassBasedInterpreterHeader || flags.mode == GenerateMode::ClassBasedInterpreterSource ||
        flags.mode == GenerateMode::EbmgenVisitor) {
        if (flags.mode == GenerateMode::ClassBasedCodeGeneratorSource || flags.mode == GenerateMode::ClassBasedInterpreterSource) {
            w.writeln("#include \"codegen.hpp\"");
        }
        else {
            if (!locations.is_codegen) {
                w.writeln("#include <ebmcodegen/stub/writer_manager.hpp>");
            }
            w.writeln("#include <concepts>");
            w.writeln("#include <strutil/append.h>");
        }
        CodeWriter dummy;
        if (flags.mode == GenerateMode::ClassBasedCodeGeneratorHeader || flags.mode == GenerateMode::ClassBasedInterpreterHeader ||
            flags.mode == GenerateMode::EbmgenVisitor) {
            ebmcodegen::generate(locations, w, dummy, struct_map);
            if (locations.ebmgen_mode) {
                w.write_unformatted(dummy.out());
            }
        }
        else {
            ebmcodegen::generate(locations, dummy, w, struct_map);
        }
        cout << w.out();
        return 0;
    }

    CodeWriter visitor_stub;
    CodeWriter visitor_assert;
    visitor_stub.writeln("struct Visitor {");
    auto visitor_scope = visitor_stub.indent_scope();
    visitor_stub.writeln("static constexpr const char* program_name = \"", ns_name, "\";");
    visitor_stub.writeln("ebmgen::MappingTable module_;");
    visitor_stub.writeln("Flags& flags;");
    visitor_stub.writeln("Output& output;");
    if (flags.mode == GenerateMode::CodeGenerator) {
        visitor_stub.writeln("futils::code::CodeWriter<futils::binary::writer&> root;");
        visitor_stub.writeln("std::vector<CodeWriter> tmp_writers;");
        visitor_stub.writeln("[[nodiscard]] auto add_writer() {");
        {
            auto scope = visitor_stub.indent_scope();
            visitor_stub.writeln("tmp_writers.emplace_back();");
            visitor_stub.writeln("return futils::helper::defer([&]() {");
            {
                auto scope = visitor_stub.indent_scope();
                visitor_stub.writeln("tmp_writers.pop_back();");
            }
            visitor_stub.writeln("});");
        }
        visitor_stub.writeln("}");
        visitor_stub.writeln("CodeWriter* get_writer() {");
        {
            auto scope = visitor_stub.indent_scope();
            visitor_stub.writeln("if (tmp_writers.empty()) {");
            {
                auto if_scope = visitor_stub.indent_scope();
                visitor_stub.writeln("return nullptr;");
            }
            visitor_stub.writeln("}");
            visitor_stub.writeln("return &tmp_writers.back();");
        }
        visitor_stub.writeln("}");

        visitor_stub.writeln("Visitor(const ebm::ExtendedBinaryModule& m,futils::binary::writer& w,Flags& f,Output& o) : module_(m), root{w}, flags{f}, output{o} {}");
    }
    else {
        visitor_stub.writeln("Visitor(const ebm::ExtendedBinaryModule& m,Flags& f,Output& o) : module_(m), flags{f}, output{o} {}");
    }

    std::vector<std::string> hooks;
    auto concat = [](auto&&... args) {
        return futils::strutil::concat<std::string>(std::forward<decltype(args)>(args)...);
    };

    auto insert_hook_without_end = [&](auto&& w, auto&& concated) {
        hooks.push_back(concated);
        w.writeln("#if __has_include(\"", flags.visitor_impl_dir, concated, ".hpp", "\")");
        w.writeln("#include \"", flags.visitor_impl_dir, concated, ".hpp", "\"");
    };

    auto insert_hook_and_dsl_without_end = [&](auto&& w, auto&& concated) {
        insert_hook_without_end(w, concated);
        auto concated_dsl = concat(concated, suffixes[suffix_dsl]);
        hooks.push_back(concated_dsl);
        w.writeln("#elif __has_include(\"", flags.visitor_impl_dsl_dir, concated_dsl, ".hpp", "\")");
        w.writeln("#include \"", flags.visitor_impl_dsl_dir, concated_dsl, ".hpp", "\"");
    };

    auto insert_include_without_endif = [&](auto&& w, auto&&... path) {
        auto main_point = concat(std::forward<decltype(path)>(path)...);
        auto before = concat(main_point, suffixes[suffix_before]);
        auto pre_default = concat(main_point, suffixes[suffix_pre_default]);
        auto post_default = concat(main_point, suffixes[suffix_post_default]);
        auto after = concat(main_point, suffixes[suffix_after]);
        {
            insert_hook_and_dsl_without_end(w, before);
            w.writeln("#endif");
        }
        insert_hook_and_dsl_without_end(w, main_point);
        {
            w.writeln("#elif __has_include(\"", flags.default_visitor_impl_dir, main_point, ".hpp", "\")");
            {
                insert_hook_and_dsl_without_end(w, pre_default);
                w.writeln("#endif");
            }
            w.writeln("#include \"", flags.default_visitor_impl_dir, main_point, ".hpp", "\"");
            {
                insert_hook_and_dsl_without_end(w, post_default);
                w.writeln("#endif");
            }
        }
        {
            insert_hook_and_dsl_without_end(w, after);
            w.writeln("#endif");
        }
    };

    std::string result_type = "expected<Result>";

    auto insert_include_inner_result_lambda = [&](auto&& w, auto&& on_nothing_implemented, auto&&... path) {
        auto main_point = concat(std::forward<decltype(path)>(path)...);
        auto before = concat(main_point, suffixes[suffix_before]);
        auto pre_default = concat(main_point, suffixes[suffix_pre_default]);
        auto post_default = concat(main_point, suffixes[suffix_post_default]);
        auto after = concat(main_point, suffixes[suffix_after]);
        w.writeln("auto generator_default_logic = [&]() -> ", result_type, " {");
        {
            auto scope = w.indent_scope();
            on_nothing_implemented();
        }
        w.writeln("};");
        w.writeln("auto default_logic = [&]() -> ", result_type, " {");
        {
            auto default_indent = w.indent_scope();
            w.writeln("#if __has_include(\"", flags.default_visitor_impl_dir, main_point, ".hpp", "\") ");
            {
                w.writeln("auto do_default = [&]() -> ", result_type, " {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("#include \"", flags.default_visitor_impl_dir, main_point, ".hpp", "\"");
                    w.writeln("return {}; // placeholder for return value");
                }
                w.writeln("};");
                {
                    insert_hook_without_end(w, pre_default);
                    w.writeln("#endif");
                }
                w.writeln("auto result = do_default();");
                {
                    insert_hook_without_end(w, post_default);
                    w.writeln("#endif");
                }
                w.writeln("return result; // placeholder for return value");
            }
            w.writeln("#else");
            w.writeln("return generator_default_logic();");
            w.writeln("#endif");
            w.writeln("return {}; // placeholder for return value");
        }
        w.writeln("};");
        w.writeln("auto main_logic = [&]() -> ", result_type, " {");
        {
            auto indent = w.indent_scope();
            insert_hook_and_dsl_without_end(w, main_point);
            w.writeln("#else");
            w.writeln("return default_logic();");
            w.writeln("#endif");
            w.writeln("return {}; // placeholder for return value");
        }
        w.writeln("};");
        insert_hook_and_dsl_without_end(w, before);
        w.writeln("#endif");
        w.writeln("auto result = main_logic();");
        insert_hook_and_dsl_without_end(w, after);
        w.writeln("#endif");
        w.writeln("return result;");
    };

    auto insert_include = [&](auto& w, auto&&... path) {
        insert_include_without_endif(w, std::forward<decltype(path)>(path)...);
        w.writeln("#endif");
    };
    insert_include(visitor_stub, prefixes[prefix_visitor]);

    visitor_stub.writeln("expected<void> entry() {");
    auto entry_scope = visitor_stub.indent_scope();
    insert_include(visitor_stub, prefixes[prefix_entry]);
    visitor_stub.writeln("return {};");  // Placeholder for entry function
    entry_scope.execute();
    visitor_stub.writeln("}");

    insert_include(w, prefixes[prefix_includes]);
    w.writeln();  // line for end of #include

    auto with_flag_bind = [&](bool on_define) {
        constexpr auto ensure_c_ident = "static_assert(ebmcodegen::util::internal::is_c_ident(#name),\"name must be a valid C identifier\");";
        w.writeln("#define DEFINE_FLAG(type,name,default_,flag_name,flag_func,...) \\");
        if (on_define) {
            w.indent_writeln(ensure_c_ident, "type name = default_");
        }
        else {
            w.indent_writeln("ctx.flag_func(&name,flag_name,__VA_ARGS__)");
        }
        w.write("#define WEB_FILTERED(...) ");
        if (!on_define) {
            w.write("web_filtered.insert_range(std::set{__VA_ARGS__})");
        }
        w.writeln();
        auto map_name = [&](auto name, auto dst, auto src) {
            w.write("#define ", name, "(", src, ") ");
            if (!on_define) {
                w.write(dst, " = ", src);
            }
            w.writeln();
        };
        map_name("WEB_UI_NAME", "ui_lang_name", "name");
        map_name("WEB_LSP_NAME", "lsp_name", "name");
        map_name("WEB_WORKER_NAME", "webworker_name", "name");
        w.write("#define FILE_EXTENSIONS(...) ");
        if (!on_define) {
            w.write("file_extensions = std::vector<std::string_view>{__VA_ARGS__}");
        }
        w.writeln();

        w.writeln("#define DEFINE_BOOL_FLAG(name,default_,flag_name,desc) DEFINE_FLAG(bool,name,default_,flag_name,VarBool,desc)");
        w.writeln("#define DEFINE_STRING_FLAG(name,default_,flag_name,desc,arg_desc) DEFINE_FLAG(std::string_view,name,default_,flag_name,VarString<true>,desc,arg_desc)");
        w.write("#define BEGIN_MAP_FLAG(name,MappedType,default_,flag_name,desc)");
        if (on_define) {
            w.write(ensure_c_ident, "MappedType name = default_;");
        }
        else {
            w.write("{ std::map<std::string,MappedType> map__; auto& target__ = name; auto flag_name__ = flag_name; auto desc__ = desc; std::string arg_desc__ = \"{\"; ");
        }
        w.writeln();
        w.write("#define MAP_FLAG_ITEM(key,value) ");
        if (!on_define) {
            w.write("map__[key] = value;");
            w.write("if (!arg_desc__.empty() && arg_desc__.back() != '{') { arg_desc__ += \",\"; }");
            w.write("arg_desc__ += key;");
        }
        w.writeln();
        w.write("#define END_MAP_FLAG() ");
        if (!on_define) {
            w.write("ctx.VarMap(&target__,flag_name__,desc__,arg_desc__ + \"}\",std::move(map__)); }");
        }
        w.writeln();

        insert_include(w, prefixes[prefix_flags]);
        w.writeln("#undef DEFINE_FLAG");
        w.writeln("#undef WEB_FILTERED");
        w.writeln("#undef DEFINE_BOOL_FLAG");
        w.writeln("#undef DEFINE_STRING_FLAG");
        w.writeln("#undef BEGIN_MAP_FLAG");
        w.writeln("#undef MAP_FLAG_ITEM");
        w.writeln("#undef END_MAP_FLAG");
        w.writeln("#undef WEB_UI_NAME");
        w.writeln("#undef WEB_LSP_NAME");
        w.writeln("#undef WEB_WORKER_NAME");
        w.writeln("#undef FILE_EXTENSIONS");
    };

    w.writeln("struct Flags : ebmcodegen::Flags {");
    {
        auto flag_scope = w.indent_scope();
        with_flag_bind(true);
        insert_include(w, prefixes[prefix_flags], suffixes[suffix_struct]);
        w.writeln("void bind(futils::cmdline::option::Context& ctx) {");
        auto nested_scope = w.indent_scope();
        w.writeln("lang_name = \"", flags.lang, "\";");
        w.writeln("ui_lang_name = lang_name;");
        w.writeln("lsp_name = lang_name;");
        w.writeln("webworker_name = \"", flags.program_name, "\";");
        w.writeln("file_extensions = {\".", flags.lang, "\"};");
        w.writeln("ebmcodegen::Flags::bind(ctx); // bind basis");
        with_flag_bind(false);
        insert_include(w, prefixes[prefix_flags], suffixes[suffix_bind]);
        nested_scope.execute();
        w.writeln("}");
    }
    w.writeln("};");
    w.writeln("struct Output : ebmcodegen::Output {");
    insert_include(w, prefixes[prefix_output]);
    w.writeln("};");

    w.writeln("namespace ", ns_name, " {");
    auto ns_scope = w.indent_scope();
    w.writeln("using namespace ebmgen;");
    w.writeln("using namespace ebmcodegen::util;");
    w.writeln("using CodeWriter = futils::code::LocWriter<std::string,std::vector,ebm::AnyRef>;");

    w.writeln();
    w.writeln("struct Result {");
    auto result_scope = w.indent_scope();
    if (flags.mode == GenerateMode::CodeGenerator) {
        w.writeln("private: CodeWriter value;");
        w.writeln("public: Result(std::string v) { value.write(v); }");
        w.writeln("Result(const char* v) { value.write(v); }");
        w.writeln("Result(CodeWriter&& v) : value(std::move(v)) {}");
        w.writeln("Result() = default;");
        w.writeln("constexpr std::string to_string() const {");
        w.indent_writeln("return value.to_string();");
        w.writeln("}");
        w.writeln("constexpr const CodeWriter& to_writer() const {");
        w.indent_writeln("return value;");
        w.writeln("}");
        w.writeln("constexpr CodeWriter& to_writer() {");
        w.indent_writeln("return value;");
        w.writeln("}");
    }
    insert_include(w, prefixes[prefix_result]);
    result_scope.execute();
    w.writeln("};");

    auto dispatcher = [&](auto t, std::string_view kind, auto subset) {
        using T = std::decay_t<decltype(t)>;
        CodeWriter stmt_dispatcher;

        auto write_visit_entry = [&](auto& w, bool decl, auto&&... name) {
            w.writeln("template<typename Visitor>");
            w.write(result_type, " ", name..., "(Visitor&& visitor,const ebm::", kind, "& in,ebm::", kind, "Ref alias_ref", decl ? " = {}" : "", ")");
        };
        write_visit_entry(w, true, "visit_", kind);
        w.writeln(";");

        auto body_name = std::string(kind) + "Body";
        auto ref_name = std::string(kind) + "Ref";

        auto make_dispatch_func_name = [&](auto v) {
            return std::format("dispatch_{}_{}", kind, to_string(v));
        };

        for (size_t i = 0; to_string(T(i))[0]; i++) {
            auto& body = struct_map[body_name];
            auto add_arguments = [&] {
                w.write("std::declval<const ebm::", ref_name, "&>()");
                for (auto& field : body.fields) {
                    if (!subset[T(i)].first.contains(field.name)) {
                        continue;
                    }
                    if (field.attr & ebmcodegen::TypeAttribute::PTR) {
                        w.write(",*std::declval<const ebm::", body_name, "&>().", field.name, "()");
                    }
                    else {
                        w.write(",std::declval<const ebm::", body_name, "&>().", field.name);
                    }
                }
            };
            auto call_arguments = [&] {
                w.write("is_nil(alias_ref) ? in.id : alias_ref");
                for (auto& field : body.fields) {
                    if (!subset[T(i)].first.contains(field.name)) {
                        continue;
                    }
                    w.write(",", field.name);
                }
            };
            auto concept_name = std::format("has_visitor_{}_{}", kind, to_string(T(i)));
            auto concept_call_name = concept_name + "_call";
            auto visitor_func_name = std::format("visit_{}_{}", kind, to_string(T(i)));
            auto dispatch_func_name = make_dispatch_func_name(T(i));

            // generating concepts
            {
                w.writeln("template<typename Visitor>");
                w.writeln("concept ", concept_name, " = requires(Visitor v) {");
                auto requires_scope = w.indent_scope();

                w.write(" { v.", visitor_func_name, "(");
                add_arguments();
                w.writeln(") } -> std::convertible_to<", result_type, ">;");
                requires_scope.execute();
                w.writeln("};");
            }
            {
                w.writeln("template<typename Visitor>");
                w.writeln("concept ", concept_call_name, " = requires(Visitor fn) {");
                auto requires_scope = w.indent_scope();
                w.write(" { fn(");
                add_arguments();
                w.writeln(") } -> std::convertible_to<", result_type, ">;");
                requires_scope.execute();
                w.writeln("};");
            }

            // generating dispatch function
            write_visit_entry(w, false, dispatch_func_name);
            w.writeln(" {");
            {
                auto scope = w.indent_scope();
                insert_include(w, kind, suffixes[suffix_pre_validate]);
                insert_include(w, kind, "_", to_string(T(i)), suffixes[suffix_pre_validate]);
                for (auto& field : body.fields) {
                    if (!subset[T(i)].first.contains(field.name)) {
                        continue;
                    }
                    if (field.attr & ebmcodegen::TypeAttribute::PTR) {
                        w.writeln("if (!in.body.", field.name, "()) {");
                        w.indent_writeln("return unexpect_error(\"Unexpected null pointer for ", body_name, "::", field.name, "\");");
                        w.writeln("}");
                        w.writeln("auto& ", field.name, " = *in.body.", field.name, "();");
                    }
                    else {
                        w.writeln("auto& ", field.name, " = in.body.", field.name, ";");
                    }
                }
                insert_include(w, kind, suffixes[suffix_pre_visit]);
                insert_include(w, kind, "_", to_string(T(i)), suffixes[suffix_pre_visit]);
                w.writeln(result_type, " result;");
                w.writeln("if constexpr (", concept_name, "<Visitor>) {");
                {
                    auto scope2 = w.indent_scope();
                    w.write("result = visitor.", visitor_func_name, "(");
                    call_arguments();
                    w.writeln(");");
                }
                w.writeln("}");
                w.writeln("else if constexpr (", concept_call_name, "<Visitor>) {");
                {
                    auto scope3 = w.indent_scope();
                    w.write("result = visitor(");
                    call_arguments();
                    w.writeln(");");
                }
                w.writeln("}");
                w.writeln("else {");
                {
                    auto scope4 = w.indent_scope();
                    w.writeln("static_assert(", concept_name, "<Visitor>||", concept_call_name, "<Visitor>, \"Visitor does not implement ", visitor_func_name, "\");");
                }
                w.writeln("}");
                insert_include(w, kind, suffixes[suffix_post_visit]);
                insert_include(w, kind, "_", to_string(T(i)), suffixes[suffix_post_visit]);
                w.writeln("if(!result) {");
                w.indent_writeln("return unexpect_error(std::move(result.error())); // for trace");
                w.writeln("}");
                w.writeln("return result;");
            }
            w.writeln("}");

            // generating visitor function
            {
                visitor_stub.write(result_type, " ", visitor_func_name, "(const ebm::", ref_name, "& item_id");
                for (auto& field : body.fields) {
                    if (!subset[T(i)].first.contains(field.name)) {
                        continue;
                    }
                    std::string typ;
                    if (field.type.contains("std::") || field.type == "bool") {
                        typ = field.type;
                    }
                    else {
                        typ = std::format("ebm::{}", field.type);
                    }
                    if (field.attr & ebmcodegen::TypeAttribute::ARRAY) {
                        typ = std::format("std::vector<{}>", typ);
                    }
                    visitor_stub.write(",const ", typ, "& ", field.name);
                }
                visitor_stub.writeln(") {");
                auto stub_include = visitor_stub.indent_scope();
                visitor_stub.writeln("auto& visitor = *this; // for forward compatibility");
                insert_include_inner_result_lambda(
                    visitor_stub,
                    [&] {
                        visitor_stub.writeln("if (flags.debug_unimplemented) {");
                        {
                            auto scope = visitor_stub.indent_scope();
                            if (flags.mode == GenerateMode::CodeGenerator) {
                                visitor_stub.writeln("return std::format(\"{{{{Unimplemented ", kind, "_", to_string(T(i)), " {}}}}}\",get_id(item_id));");
                            }
                        }
                        visitor_stub.writeln("}");
                        visitor_stub.writeln("return {};");
                    },
                    kind, "_", to_string(T(i)));
                stub_include.execute();
                visitor_stub.writeln("}");
                visitor_assert.writeln("static_assert(", concept_name, "<Visitor>, \"Visitor does not implement ", visitor_func_name, "\");");
            }
        }

        {
            // generating dispatcher function
            write_visit_entry(stmt_dispatcher, false, "visit_", kind);
            stmt_dispatcher.writeln(" {");
            auto scope_ = stmt_dispatcher.indent_scope();
            insert_include_inner_result_lambda(
                stmt_dispatcher,
                [&] {
                    stmt_dispatcher.writeln("switch (in.body.kind) {");
                    // generating entry point of dispatch function
                    for (size_t i = 0; to_string(T(i))[0]; i++) {
                        auto dispatch_func_name = make_dispatch_func_name(T(i));
                        stmt_dispatcher.writeln("case ebm::", visit_enum(t), "::", to_string(T(i)), ":");
                        stmt_dispatcher.indent_writeln("return ", dispatch_func_name, "(visitor,in,alias_ref);");
                    }
                    stmt_dispatcher.writeln("default:");
                    stmt_dispatcher.indent_writeln("return unexpect_error(\"Unknown ", kind, " kind: {}\", to_string(in.body.kind));");
                    stmt_dispatcher.writeln("}");
                },
                kind, suffixes[suffix_dispatch]);
            scope_.execute();
            stmt_dispatcher.writeln("}");
        }

        auto lowered_kind = std::string(kind);
        lowered_kind[0] = std::tolower(kind[0]);
        stmt_dispatcher.writeln("// short-hand visitor for ", ref_name);
        stmt_dispatcher.writeln("template<typename Visitor>");
        stmt_dispatcher.writeln(result_type, " visit_", kind, "(Visitor&& visitor,const ebm::", ref_name, "& ref) {");
        {
            auto scope = stmt_dispatcher.indent_scope();
            stmt_dispatcher.writeln("MAYBE(elem, visitor.module_.get_", lowered_kind, "(ref));");
            stmt_dispatcher.writeln("return visit_", kind, "(visitor,elem,ref);");
        }
        stmt_dispatcher.writeln("}");

        stmt_dispatcher.writeln("// generic visitor for ", ref_name);
        stmt_dispatcher.writeln("template<typename Visitor>");
        stmt_dispatcher.writeln(result_type, " visit_Object(Visitor&& visitor,const ebm::", ref_name, "& ref) {");
        {
            auto scope = stmt_dispatcher.indent_scope();
            stmt_dispatcher.writeln("return visit_", kind, "(visitor,ref);");
        }
        stmt_dispatcher.writeln("}");

        auto list_name = std::format("{}s", kind);
        if (kind == "Statement") {
            list_name = "Block";
        }

        stmt_dispatcher.writeln("template<typename Visitor>");
        stmt_dispatcher.writeln(result_type, " visit_", list_name, "(Visitor&& visitor,const ebm::", list_name, "& in) {");
        {
            auto list_scope = stmt_dispatcher.indent_scope();
            insert_include_inner_result_lambda(
                stmt_dispatcher,
                [&] {
                    if (flags.mode == GenerateMode::CodeGenerator) {
                        stmt_dispatcher.writeln("CodeWriter w;");
                    }
                    stmt_dispatcher.writeln("for(auto& elem:in.container) {");
                    {
                        auto loop_scope = stmt_dispatcher.indent_scope();
                        stmt_dispatcher.writeln("auto result = visit_", kind, "(visitor,elem);");
                        stmt_dispatcher.writeln("if (!result) {");
                        stmt_dispatcher.indent_writeln("return unexpect_error(std::move(result.error()));");
                        stmt_dispatcher.writeln("}");
                        if (flags.mode == GenerateMode::CodeGenerator) {
                            stmt_dispatcher.writeln("merge_result(visitor, w, std::move(result.value()));");
                        }
                    }
                    stmt_dispatcher.writeln("}");
                    if (flags.mode == GenerateMode::CodeGenerator) {
                        stmt_dispatcher.writeln("return w;");
                    }
                    else {
                        stmt_dispatcher.writeln("return {};");  // Placeholder for non-codegen mode
                    }
                },
                list_name, suffixes[suffix_dispatch]);
            list_scope.execute();
        }
        stmt_dispatcher.writeln("}");

        stmt_dispatcher.writeln("// generic visitor for ", list_name);
        stmt_dispatcher.writeln("template<typename Visitor>");
        stmt_dispatcher.writeln(result_type, " visit_Object(Visitor&& visitor,const ebm::", list_name, "& in) {");
        {
            auto scope = stmt_dispatcher.indent_scope();
            stmt_dispatcher.writeln("return visit_", list_name, "(visitor,in);");
        }
        stmt_dispatcher.writeln("}");
        w.write_unformatted(stmt_dispatcher.out());
    };

    dispatcher(ebm::StatementKind{}, prefixes[prefix_statement], ebmcodegen::body_subset_StatementBody());
    dispatcher(ebm::ExpressionKind{}, prefixes[prefix_expression], ebmcodegen::body_subset_ExpressionBody());
    dispatcher(ebm::TypeKind{}, prefixes[prefix_type], ebmcodegen::body_subset_TypeBody());

    visitor_scope.execute();
    visitor_stub.writeln("};");
    w.write_unformatted(visitor_stub.out());
    w.write_unformatted(visitor_assert.out());

    ns_scope.execute();
    w.writeln("}  // namespace ", ns_name);

    w.writeln("DEFINE_ENTRY(Flags,Output) {");
    auto main_scope = w.indent_scope();
    insert_include(w, prefixes[prefix_pre_visitor]);
    if (flags.mode == GenerateMode::CodeGenerator) {
        w.writeln(ns_name, "::Visitor visitor{ebm,w,flags,output};");
    }
    else {
        w.writeln(ns_name, "::Visitor visitor{ebm,flags,output};");
    }
    insert_include(w, prefixes[prefix_pre_entry]);
    w.writeln("auto result = visitor.entry();");
    w.writeln("if (!result) {");
    auto err_scope = w.indent_scope();
    w.writeln("futils::wrap::cerr_wrap() << \"error: \" << result.error().error();");
    w.writeln("return 1;");
    err_scope.execute();
    w.writeln("}");
    insert_include(w, prefixes[prefix_post_entry]);
    w.writeln("return 0;");
    main_scope.execute();
    w.writeln("}");

    std::unordered_set<std::string> uniq;

    if (flags.mode == GenerateMode::HookList) {
        auto class_based_hooks = ebmcodegen::class_based_hook_names(struct_map, locations);
        for (auto& hook : hooks) {
            if (!uniq.insert(hook).second) {
                continue;
            }
            cout << hook << "\n";
        }
        for (auto& hook : class_based_hooks) {
            if (!uniq.insert(hook).second) {
                continue;
            }
            cout << hook << "\n";
        }
        return 0;
    }

    if (flags.mode == GenerateMode::Template) {
        if (flags.template_target.empty()) {
            cerr << "Requires template --template-target option: use --mode hooklist to see what kind exists.\n";
            return 1;
        }
        auto class_based_hooks = ebmcodegen::class_based_hook_names(struct_map, locations);
        auto target = flags.template_target;
        if (std::find(hooks.begin(), hooks.end(), target) == hooks.end() &&
            std::find(class_based_hooks.begin(), class_based_hooks.end(), target) == class_based_hooks.end()) {
            cerr << "No such template: " << flags.template_target << ": see --mode hooklist for available templates.\n";
            return 1;
        }
        auto result = parse_hook_name(flags.template_target, struct_map);
        if (!result) {
            cerr << "Failed to parse template: " << flags.template_target << ": " << result.error().error() << "; THIS IS BUG\n";
            return 1;
        }
        w = CodeWriter{"  "};
        w.writeln("/*license*/");
        w.writeln("// Template generated by ebmcodegen at ", repo_url);
        if (result->class_based) {
            auto res = ebmcodegen::class_based_hook_descriptions(w, *result, struct_map);
            if (!res) {
                // this is also a valid behavior
                cerr << "Failed to parse template: " << flags.template_target << ": " << res.error().error();
                return 1;
            }
        }
        else {
            w.writeln("/*");
            auto print_struct = [&](auto&& print_struct, std::string_view type) -> void {
                if (auto found = struct_map.find(type); found != struct_map.end()) {
                    auto nest = w.indent_scope();
                    for (auto& field : found->second.fields) {
                        w.write(field.name, ": ");
                        if (field.attr & ebmcodegen::TypeAttribute::PTR) {
                            w.write("*");
                        }
                        if (field.attr & ebmcodegen::TypeAttribute::ARRAY) {
                            w.write("std::vector<", field.type, ">");
                        }
                        else {
                            w.write(field.type);
                        }
                        w.writeln();
                        if (field.type != "Varint" && !field.type.ends_with("Ref")) {
                            print_struct(print_struct, field.type);
                        }
                    }
                }
            };
            {
                auto indent = w.indent_scope();
                w.writeln("Name: ", flags.template_target);
                w.writeln("Available variables:");
                auto indent2 = w.indent_scope();
                futils::helper::DynDefer ctx;
                if (result->target == prefixes[prefix_pre_visitor]) {
                    w.writeln("ebm :ExtendedBinaryModule");
                    w.writeln("flags :Flags");
                    w.writeln("output :Output");
                }
                if (result->target == prefixes[prefix_pre_entry] ||
                    result->target == prefixes[prefix_post_entry]) {
                    w.writeln("visitor: Visitor");
                }
                if (result->target == prefixes[prefix_flags]) {
                    if (result->flags_suffix == suffixes[suffix_bind]) {
                        w.writeln("ctx: futils::cmdline::option::Context&");
                        w.writeln("lang_name: const char*");
                        w.writeln("ui_lang_name: const char*");
                        w.writeln("lsp_name: const char*");
                        w.writeln("worker_name: const char*");
                        w.writeln("web_filtered: std::set<std::string>");
                    }
                    else if (result->flags_suffix.empty()) {
                        w.writeln("These are macros. Do not use other than these");
                        w.writeln("flag name format: long-name[,short-name,...] (e.g: \"flag-name,f\")");
                        w.writeln("DEFINE_FLAG(type,name,default,flag_name,flag_func,flag_func_args...)");
                        w.writeln("WEB_FILTERED(filtered_flag_names...)");
                        w.writeln("DEFINE_BOOL_FLAG(name,default,flag_name,help)");
                        w.writeln("DEFINE_STRING_FLAG(name,default,flag_name,help,arg_description)");
                        w.writeln("DEFINE_INT_FLAG(name,type,default,flag_name,help,arg_description)");
                        w.writeln("BEGIN_MAP_FLAG(name,MappedType,default,flag_name,help)");
                        w.writeln("  MAP_FLAG_ITEM(key,value) // repeat this line for each item");
                        w.writeln("END_MAP_FLAG()");
                        w.writeln("WEB_UI_NAME(ui_name)");
                        w.writeln("WEB_LSP_NAME(lsp_name)");
                        w.writeln("WEB_WORKER_NAME(worker_name)");
                        w.writeln("FILE_EXTENSIONS(file_ext...)");
                        w.writeln("CONFIG_MAP(config_name,flag_name)");
                        w.writeln("WEB_OPTION_HANDLE_TYPE(flag_name,web_type)");
                    }
                }
                if (result->visitor_location == suffixes[suffix_pre_validate] ||
                    result->visitor_location == suffixes[suffix_pre_visit] ||
                    result->visitor_location == suffixes[suffix_post_visit] ||
                    result->visitor_location == suffixes[suffix_dispatch]) {
                    w.writeln("visitor: Visitor");
                    w.indent_writeln("module_: MappingTable");
                    w.writeln("alias_ref :", result->target, "Ref");
                    w.writeln("in: ", result->target);
                    print_struct(print_struct, result->target);
                }
                if (result->target == prefixes[prefix_entry]) {
                    w.writeln("visitor: Visitor");
                    w.writeln("module_: MappingTable");
                }
                if (result->struct_info) {
                    if (result->visitor_location != suffixes[suffix_pre_validate]) {
                        if (result->visitor_location.empty()) {
                            w.writeln("visitor: Visitor");
                            w.writeln("module_: MappingTable");
                            w.writeln("item_id: ", result->target, "Ref");
                            w.writeln("generator_default_logic: std::function<expected<Result>()>");
                            if (result->include_location == suffixes[suffix_pre_default] || result->include_location == suffixes[suffix_post_default]) {
                                w.writeln("do_default: std::function<expected<Result>()>");
                            }
                            else {
                                w.writeln("default_logic: std::function<expected<Result>()>");
                            }
                            if (result->include_location == suffixes[suffix_before] || result->include_location == suffixes[suffix_after]) {
                                w.writeln("main_logic: std::function<expected<Result>()>");
                            }
                            if (result->include_location == suffixes[suffix_after] || result->include_location == suffixes[suffix_post_default]) {
                                w.writeln("result: expected<Result>");
                            }
                        }
                        if (result->visitor_location == suffixes[suffix_post_visit]) {
                            w.writeln("result: expected<Result>");
                        }
                        for (auto& field : result->struct_info->fields) {
                            if (!result->body_subset.contains(field.name)) {
                                continue;
                            }
                            w.write(field.name, ": ");
                            if (field.attr & ebmcodegen::TypeAttribute::ARRAY) {
                                w.write("std::vector<", field.type, ">");
                            }
                            else {
                                w.write(field.type);
                            }
                            w.writeln();
                            if (field.type != "Varint" && !field.type.ends_with("Ref")) {
                                print_struct(print_struct, field.type);
                            }
                        }
                    }
                }
            }
            w.writeln("*/");
        }
        cout << w.out();
        return 0;
    }

    cout << w.out();
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags,
        [](auto&& str, bool err) {
            if (err)
                cerr << str;
            else
                cout << str;
        },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}