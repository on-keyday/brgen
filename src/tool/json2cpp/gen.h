/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/writer/section.h>
#include <core/ast/tool/tool.h>
#include <core/common/expected.h>
#include <helper/transform.h>
#include <set>
#include <bitset>

namespace json2cpp {
    namespace ast = brgen::ast;
    namespace tool = ast::tool;
    struct Config {
        std::string namespace_;
        std::set<std::string> includes;
        std::set<std::string> exports;
        std::set<std::string> internals;
    };

    struct Generator {
        brgen::writer::Writer w;
        std::shared_ptr<ast::Program> root;
        Config config;
        ast::tool::Evaluator eval;
        static constexpr auto empty_void = utils::helper::either::empty_value<void>();

        Generator(std::shared_ptr<ast::Program> root)
            : root(std::move(root)) {}

        brgen::result<void> handle_config(ast::tool::ConfigDesc& conf) {
            if (conf.name == "config.export") {
                auto count = conf.arguments.size();
                for (size_t i = 0; i < count; i++) {
                    auto ident = tool::get_config_value<tool::EResultType::ident>(eval, conf, ast::tool::ValueStyle::call, i, count);
                    if (!ident) {
                        return ident.transform(empty_void);
                    }
                    config.exports.emplace(ident.value());
                }
            }
            else if (conf.name == "config.internal") {
                auto count = conf.arguments.size();
                for (size_t i = 0; i < count; i++) {
                    auto ident = tool::get_config_value<tool::EResultType::ident>(eval, conf, ast::tool::ValueStyle::call, i, count);
                    if (!ident) {
                        return ident.transform(empty_void);
                    }
                    config.internals.emplace(ident.value());
                }
            }
            else if (conf.name == "config.cpp.namespace") {
                auto r = tool::get_config_value<tool::EResultType::string>(eval, conf);
                if (!r) {
                    return r.transform(empty_void);
                }
                config.namespace_ = r.value();
            }
            else if (conf.name == "config.cpp.include") {
                if (conf.arguments.size() == 2) {
                    auto r = tool::get_config_value<tool::EResultType::boolean>(eval, conf, ast::tool::ValueStyle::call, 1, 2);
                    if (!r) {
                        return r.transform(empty_void);
                    }
                    if (*r) {
                        auto r = tool::get_config_value<tool::EResultType::string>(eval, conf, ast::tool::ValueStyle::call, 0, 2);
                        if (!r) {
                            return r.transform(empty_void);
                        }

                        config.includes.emplace("<" + brgen::escape(*r) + ">");
                    }
                    else {
                        auto r = tool::get_config_value<tool::EResultType::string, false>(eval, conf, ast::tool::ValueStyle::call, 0, 2);
                        if (!r) {
                            return r.transform(empty_void);
                        }
                        config.includes.emplace(std::move(*r));
                    }
                }
                else {
                    auto r = tool::get_config_value<tool::EResultType::string, false>(eval, conf, ast::tool::ValueStyle::call);
                    if (!r) {
                        return r.transform(empty_void);
                    }
                    config.includes.emplace(std::move(*r));
                }
            }
            return {};
        }

        brgen::result<void> generate_format(const std::shared_ptr<ast::Format>& fmt) {
            auto name = fmt->ident->ident;
            auto& fields = fmt->struct_type->fields;
            for (auto& f : fields) {
                if (auto field = ast::as<ast::Field>(f)) {
                    auto fname = field->ident->ident;
                    if (auto arr = tool::is_array_type(field->field_type, eval)) {
                    }
                }
            }
            return {};
        }

        brgen::result<void> generate() {
            std::vector<std::shared_ptr<ast::Format>> formats;
            for (auto& l : root->elements) {
                if (l->node_type == ast::NodeType::format) {
                    formats.push_back(std::static_pointer_cast<ast::Format>(l));
                }
                else if (auto conf = tool::extract_config(l)) {
                    auto r = handle_config(conf.value());
                    if (!r) {
                        return r.transform(empty_void);
                    }
                }
            }
            for (auto& fmt : formats) {
                auto r = generate_format(fmt);
                if (!r) {
                    return r;
                }
            }
            return {};
        }
    };
}  // namespace json2cpp
