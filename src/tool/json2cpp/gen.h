/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/writer/section.h>
#include <core/ast/tool/tool.h>
#include <core/common/expected.h>
#include <helper/transform.h>
#include <set>
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

    enum class DescType {
        array,
        vector,
        int_,
    };

    struct Desc {
        const DescType type;
        constexpr Desc(DescType type)
            : type(type) {}
    };

    struct IntDesc : Desc {
        tool::IntDesc desc;
        constexpr IntDesc(tool::IntDesc desc)
            : Desc(DescType::int_), desc(desc) {}
    };

    struct ArrayDesc : Desc {
        tool::ArrayDesc desc;
        std::shared_ptr<Desc> base_type;
        ArrayDesc(tool::ArrayDesc desc, std::shared_ptr<Desc> base_type)
            : Desc(DescType::array), desc(desc), base_type(std::move(base_type)) {}
    };

    struct Field;

    struct VectorDesc : Desc {
        tool::ArrayDesc desc;
        std::shared_ptr<Desc> base_type;
        std::shared_ptr<ast::Expr> resolved_expr;
        VectorDesc(tool::ArrayDesc desc, std::shared_ptr<Desc> base_type)
            : Desc(DescType::vector), desc(desc), base_type(std::move(base_type)) {}
    };

    struct Field {
        std::shared_ptr<ast::Field> base;
        std::shared_ptr<Desc> desc;
        std::weak_ptr<Field> length_related;

        Field(std::shared_ptr<ast::Field> base, std::shared_ptr<Desc> desc)
            : base(std::move(base)), desc(std::move(desc)) {}
    };

    // read/write by binary::[write/read]_num_bulk
    // only contains integer or array of integer
    struct BulkFields {
        std::vector<std::shared_ptr<Field>> fields;
        size_t fixed_size = 0;
    };

    struct Fields {
        std::vector<std::shared_ptr<Field>> fields;

        void push(std::shared_ptr<Field>&& f) {
            fields.push_back(std::move(f));
        }
    };

    struct MergedFields {
        std::vector<std::variant<std::shared_ptr<Field>, BulkFields>> fields;
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

        brgen::result<void> collect_field(Fields& f, std::shared_ptr<ast::Field>&& field) {
            auto& fname = field->ident->ident;
            if (auto a = tool::is_array_type(field->field_type, eval)) {
                auto b = tool::is_int_type(a->base_type);
                if (!b) {
                    return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
                }
                auto type = get_primitive_type(b->bit_size);
                if (type.empty()) {
                    return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
                }
                if (a->length_eval) {
                    config.includes.emplace("<array>");
                    f.push(std::make_shared<Field>(field, std::make_shared<ArrayDesc>(std::move(*a), std::make_shared<IntDesc>(std::move(*b)))));
                }
                else {
                    config.includes.emplace("<vector>");
                    auto vec = std::make_shared<VectorDesc>(std::move(*a), std::make_shared<IntDesc>(std::move(*b)));
                    f.push(std::make_shared<Field>(field, std::move(vec)));
                }
            }
            else if (auto i = tool::is_int_type(field->field_type)) {
                auto type = get_primitive_type(i->bit_size);
                if (type.empty()) {
                    return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
                }
                f.push(std::make_shared<Field>(field, std::make_shared<IntDesc>(std::move(*i))));
            }
            else {
                return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
            }
            return {};
        }

        brgen::result<void> merge_fields(MergedFields& m, Fields& f) {
            for (size_t i = 0; i < f.fields.size(); i++) {
                if (f.fields[i]->desc->type == DescType::vector) {
                    m.fields.push_back({std::move(f.fields[i])});
                    continue;
                }
                BulkFields b;
                for (; i < f.fields.size(); i++) {
                    if (f.fields[i]->desc->type == DescType::array) {
                        b.fields.push_back({std::move(f.fields[i])});
                    }
                    else if (f.fields[i]->desc->type == DescType::int_) {
                        b.fields.push_back({std::move(f.fields[i])});
                    }
                    else {
                        if (b.fields.size() == 1) {
                            m.fields.push_back({std::move(b.fields[0])});
                        }
                        else {
                            m.fields.push_back({std::move(b)});
                        }
                        i--;
                        break;
                    }
                }
            }
            return {};
        }

        brgen::result<void> generate_field(const Field& f) {
            if (f.desc->type == DescType::int_) {
                auto size = static_cast<IntDesc*>(f.desc.get())->desc.bit_size;
                w.writeln(get_primitive_type(size), " ", f.base->ident->ident, ";");
            }
            else {
                auto desc = static_cast<ArrayDesc*>(f.desc.get());
                auto size = static_cast<IntDesc*>(desc->base_type.get())->desc.bit_size;
                auto& len = desc->desc.length_eval;
                if (len) {
                    w.writeln("std::array<", get_primitive_type(size), ",", brgen::nums(len->get<tool::EResultType::integer>()), "> ", f.base->ident->ident, ";");
                }
                else {
                    w.writeln("std::vector<", get_primitive_type(size), "> ", f.base->ident->ident, ";");
                }
            }
            return {};
        }

        // generate field
        brgen::result<void> generate_fields(const MergedFields& f) {
            for (auto& field : f.fields) {
                if (auto f = std::get_if<BulkFields>(&field)) {
                    for (auto& field : f->fields) {
                        if (auto res = generate_field(*field); !res) {
                            return res.transform(empty_void);
                        }
                    }
                }
                else if (auto f = std::get_if<std::shared_ptr<Field>>(&field)) {
                    if (auto res = generate_field(**f); !res) {
                        return res.transform(empty_void);
                    }
                }
            }
            return {};
        }

        brgen::result<void> generate_encoder(MergedFields& f, bool decoder, std::string_view num_method) {
            constexpr auto is_be = "true";
            w.writeln("constexpr bool render(::utils::binary::writer& w) const {");
            {
                auto sc = w.indent_scope();
                for (auto& field : f.fields) {
                    if (BulkFields* f = std::get_if<BulkFields>(&field)) {
                        w.write("if(!", num_method, "_bulk(w,", is_be);
                        for (auto& field : f->fields) {
                            if (field->desc->type == DescType::array) {
                                auto len = static_cast<ArrayDesc*>(field->desc.get())->desc.length_eval->get<tool::EResultType::integer>();
                                for (size_t i = 0; i < len; i++) {
                                    w.write(",", field->base->ident->ident, "[", brgen::nums(i), "]");
                                }
                            }
                            else {
                                w.write(",", field->base->ident->ident);
                            }
                        }
                        w.writeln(")) {");
                        w.indent_writeln("return false;");
                        w.writeln("}");
                    }
                    else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                        auto& f = *fp;
                        if (f->desc->type == DescType::array) {
                            auto desc = static_cast<ArrayDesc*>(f->desc.get());

                            auto len = desc->desc.length_eval->get<tool::EResultType::integer>();
                            w.writeln("if(!", num_method, "_bulk", "(w,", is_be);
                            for (size_t i = 0; i < len; i++) {
                                w.write(",", f->base->ident->ident, "[", brgen::nums(i), "]");
                            }
                            w.writeln(");");
                        }
                        else if (f->desc->type == DescType::vector) {
                            auto desc = static_cast<VectorDesc*>(f->desc.get());
                            if (decoder) {
                                tool::Stringer s;
                                s.to_string(desc->desc.length);
                                w.writeln(f->base->ident->ident, ".resize(", s.buffer, ");");
                            }
                            auto i = "i_" + f->base->ident->ident;
                            w.writeln("for(size_t ", i, " = 0;", i, " < ", f->base->ident->ident, ".size();", i, "++) {");
                            {
                                auto sc = w.indent_scope();
                                w.writeln("if(!", num_method, "(w,", f->base->ident->ident, "[", i, "]", ",", is_be, ")) {");
                                w.indent_writeln("return false;");
                                w.writeln("}");
                            }
                            w.writeln("}");
                        }
                        else {
                            w.writeln("if(!", num_method, "(w,", f->base->ident->ident, ",", is_be, ")) {");
                            w.indent_writeln("return false;");
                            w.writeln("}");
                        }
                    }
                }
            }
            w.writeln("}");
            return {};
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
                        if (auto r = collect_field(fs, ast::cast_to<ast::Field>(f)); !r) {
                            return r.transform(empty_void);
                        }
                    }
                }

                MergedFields mfs;
                if (auto r = merge_fields(mfs, fs); !r) {
                    return r.transform(empty_void);
                }

                if (auto r = generate_fields(mfs); !r) {
                    return r.transform(empty_void);
                }

                if (mfs.fields.size() > 0) {
                    if (auto r = generate_encoder(mfs, false, "::utils::binary::write_num"); !r) {
                        return r.transform(empty_void);
                    }
                    w.writeln();
                    if (auto r = generate_encoder(mfs, true, "::utils::binary::read_num"); !r) {
                        return r.transform(empty_void);
                    }
                }
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
