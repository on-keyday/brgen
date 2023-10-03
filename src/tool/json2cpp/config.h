/*license*/
#pragma once
#include <core/ast/tool/tool.h>
#include <core/common/expected.h>
#include <helper/transform.h>

namespace json2cpp {
    namespace ast = brgen::ast;
    namespace tool = ast::tool;
    enum class VectorMode {
        std_vector,
        pointer,
    };

    static constexpr auto empty_void = utils::helper::either::empty_value<void>();

    struct Config {
        std::string namespace_;
        std::set<std::string> includes;
        std::set<std::string> exports;
        VectorMode vector_mode = VectorMode::std_vector;
        bool asymmetric = false;

        brgen::result<void> handle_config(tool::Evaluator& eval, ast::tool::ConfigDesc& conf) {
            if (conf.name == "config.export") {
                auto count = conf.arguments.size();
                for (size_t i = 0; i < count; i++) {
                    auto ident = tool::get_config_value<tool::EResultType::ident>(eval, conf, ast::tool::ValueStyle::call, i, count);
                    if (!ident) {
                        return ident.transform(empty_void);
                    }
                    exports.emplace(ident.value());
                }
            }
            else if (conf.name == "config.cpp.namespace") {
                auto r = tool::get_config_value<tool::EResultType::string>(eval, conf);
                if (!r) {
                    return r.transform(empty_void);
                }
                namespace_ = r.value();
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
                        includes.emplace("<" + brgen::escape(*r) + ">");
                    }
                    else {
                        auto r = tool::get_config_value<tool::EResultType::string, false>(eval, conf, ast::tool::ValueStyle::call, 0, 2);
                        if (!r) {
                            return r.transform(empty_void);
                        }
                        includes.emplace(std::move(*r));
                    }
                }
                else {
                    auto r = tool::get_config_value<tool::EResultType::string, false>(eval, conf, ast::tool::ValueStyle::call);
                    if (!r) {
                        return r.transform(empty_void);
                    }
                    includes.emplace(std::move(*r));
                }
            }
            else if (conf.name == "config.cpp.vector_mode") {
                auto r = tool::get_config_value<tool::EResultType::string>(eval, conf);
                if (!r) {
                    return r.transform(empty_void);
                }
                if (*r == "std_vector") {
                    vector_mode = VectorMode::std_vector;
                }
                else if (*r == "pointer") {
                    vector_mode = VectorMode::pointer;
                }
                else {
                    return brgen::unexpect(brgen::error(conf.loc, "invalid vector mode"));
                }
            }
            else if (conf.name == "config.cpp.vector_pointer.asymmetric") {
                auto r = tool::get_config_value<tool::EResultType::boolean>(eval, conf);
                if (!r) {
                    return r.transform(empty_void);
                }
                asymmetric = *r;
            }
            return {};
        }
    };
}  // namespace json2cpp
