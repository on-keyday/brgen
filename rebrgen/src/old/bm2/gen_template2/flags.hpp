/*license*/
#pragma once
#include <cmdline/template/help_option.h>
#include <string>
#include <vector>
#include <map>

enum class GenerateMode {
    generator,
    header,
    main,
    cmake,
    docs_json,
    docs_markdown,
};

struct CommandLineOptions {
    GenerateMode mode = GenerateMode::generator;
    std::string_view lang_name;
    std::string_view config_file = "config.json";
    std::string_view hook_file_dir = "hook";
    bool debug = false;
    bool print_hooks = false;

    void bind(futils::cmdline::option::Context& ctx) {
        ctx.VarString<true>(&lang_name, "lang", "language name", "LANG");
        ctx.VarBool(&debug, "debug", "debug mode (print hook call on stderr)");
        ctx.VarBool(&print_hooks, "print-hooks", "print hooks");
        ctx.VarString<true>(&config_file, "config-file", "config file", "FILE");
        ctx.VarString<true>(&hook_file_dir, "hook-dir", "hook file directory", "DIR");
        ctx.VarMap<std::string, GenerateMode, std::map>(
            &mode, "mode", "generate mode (generator,header,main,cmake)", "MODE",
            std::map<std::string, GenerateMode>{
                {"generator", GenerateMode::generator},
                {"header", GenerateMode::header},
                {"main", GenerateMode::main},
                {"cmake", GenerateMode::cmake},
                {"docs-json", GenerateMode::docs_json},
                {"docs-markdown", GenerateMode::docs_markdown},
            });
    }
};

struct LanguageConfig {
    // TODO: Add language-specific configuration options here
};

struct EnvMapping {
    std::string param_name;
    std::map<std::string, std::string> map;
    std::string file_name;
    std::string func_name;
    size_t line;
};

struct FlagUsageMapping {
    std::string flag_name;
    std::string value;
    std::string file_name;
    std::string func_name;
    size_t line;
};

struct DocEntry {
    std::vector<EnvMapping> env_mappings;
    std::vector<FlagUsageMapping> flag_usage_mappings;
};

struct DocumentationData {
    std::map<std::string, std::map<std::string, DocEntry>> content;
    std::string current_func_name;
};

struct Flags : futils::cmdline::templ::HelpOption {
    CommandLineOptions cmd_options;
    LanguageConfig lang_config;
    DocumentationData doc_data;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        cmd_options.bind(ctx);
    }
};
