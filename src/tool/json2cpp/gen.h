/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/writer/section.h>
#include <core/ast/tool/tool.h>
#include <core/common/expected.h>
#include <helper/transform.h>
#include <set>
#include <bitset>
#include <binary/writer.h>

namespace json2cpp {
    namespace ast = brgen::ast;
    namespace tool = ast::tool;
    struct Config {
        std::string namespace_;
        std::set<std::string> includes;
        std::set<std::string> exports;
        std::set<std::string> internals;
    };

    struct Field {
        std::shared_ptr<ast::Field> base;
        std::optional<tool::ArrayDesc> a_desc;
        std::optional<tool::IntDesc> i_desc;
    };

    struct BulkFields {
        std::vector<Field> fields;
        size_t fixed_size = 0;
    };

    struct MergedField {
        std::variant<Field, BulkFields> field;
    };

    struct Fields {
        std::vector<MergedField> merged_fields;
        std::vector<Field> cur_fields;
        size_t fixed_size = 0;
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

        auto get_primitive_type(size_t bit_size) -> std::string_view {
            switch (bit_size) {
                case 8:
                    return "std::int8_t";
                case 16:
                    return "std::int16_t";
                case 32:
                    return "std::int32_t";
                case 64:
                    return "std::int64_t";
                default:
                    return {};
            }
        }

        // generate field
        brgen::result<void> generate_field(Fields& f, std::shared_ptr<ast::Field>&& field) {
            auto& fname = field->ident->ident;

            if (auto i = tool::is_array_type(field->field_type, eval)) {
                auto b = tool::is_int_type(i->base_type);
                if (!b) {
                    return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
                }
                auto type = get_primitive_type(b->bit_size);
                if (type.empty()) {
                    return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
                }
                if (i->length_eval) {
                    w.writeln(type, " ", fname, "[", brgen::nums(i->length_eval->get<tool::EResultType::integer>()), "];");
                    f.fixed_size += i->length_eval->get<tool::EResultType::integer>() * b->bit_size / 8;
                }
                else {
                    if (f.cur_fields.size()) {
                        f.merged_fields.push_back({.field = BulkFields{std::move(f.cur_fields)}});
                        f.cur_fields.clear();
                    }
                    w.writeln("std::vector<", type, "> ", fname, ";");
                }
                f.cur_fields.push_back({field, i, b});
            }
            else if (auto i = tool::is_int_type(field->field_type)) {
                auto type = get_primitive_type(i->bit_size);
                if (type.empty()) {
                    return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
                }
                w.writeln(type, " ", fname, ";");
                f.fixed_size += i->bit_size / 8;
                f.cur_fields.push_back({field, std::nullopt, i});
            }
            else {
                return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
            }
            return {};
        }

        brgen::result<void> generate_encoder(Fields& f) {
            w.writeln("constexpr bool render(::utils::binary::writer& w) const {");
            {
                auto sc = w.indent_scope();
                w.write("return");
                w.write("::utils::binary::write_num_bulk(w,", "true");
                for (auto& field : f.fields) {
                    if (field.a_desc) {
                        if (field.a_desc->length_eval) {
                            auto len = field.a_desc->length_eval->get<tool::EResultType::integer>();
                            for (size_t i = 0; i < len; i++) {
                                w.write(",", field.base->ident->ident, "[", brgen::nums(i), "]");
                            }
                        }
                    }
                    else {
                        w.write(",", field.base->ident->ident);
                    }
                }
                w.writeln(");");
            }
            w.writeln("}");
        }

        brgen::result<void> generate_decoder(Fields& f) {
            w.writeln("constexpr bool parse(::utils::binary::reader& r) {");
            {
                auto sc = w.indent_scope();
                w.write("return ::utils::binary::read_num_bulk(r,", "true");
                for (auto& f : f.fields) {
                    if (auto field = ast::as<ast::Field>(f)) {
                        if (auto arr = tool::is_array_type(field->field_type, eval)) {
                            auto len = arr->length_eval->get<tool::EResultType::integer>();
                            for (size_t i = 0; i < len; i++) {
                                w.write(",", field->ident->ident, "[", brgen::nums(i), "]");
                            }
                        }
                        else {
                            w.write(",", field->ident->ident);
                        }
                    }
                }
                w.writeln(");");
            }
            w.writeln("}");
        }

        brgen::result<void> generate_format(const std::shared_ptr<ast::Format>& fmt) {
            auto name = fmt->ident->ident;
            auto& fields = fmt->struct_type->fields;
            Fields fs;
            w.writeln("struct ", name, " {");
            {
                auto sc = w.indent_scope();

                for (auto& f : fields) {
                    if (ast::as<ast::Field>(f)) {
                        if (auto r = generate_field(fs, ast::cast_to<ast::Field>(f)); !r) {
                            return r.transform(empty_void);
                        }
                    }
                }

                w.writeln("constexpr std::size_t size() const {");
                w.indent_writeln("return ", brgen::nums(fs.fixed_size), ";");
                w.writeln("}");
            }
            w.writeln("};");
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
            config.includes.emplace("<cstdint>");
            config.includes.emplace("<binary/number.h>");
            for (auto& inc : config.includes) {
                w.writeln("#include ", inc);
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
