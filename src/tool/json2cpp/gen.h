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
    enum class VectorMode {
        std_vector,
        pointer,
    };

    struct Config {
        std::string namespace_;
        std::set<std::string> includes;
        std::set<std::string> exports;
        std::set<std::string> internals;
        VectorMode vector_mode = VectorMode::std_vector;
    };

    enum class DescType {
        array_int,
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

    struct IntArrayDesc : Desc {
        tool::ArrayDesc desc;
        std::shared_ptr<Desc> base_type;
        IntArrayDesc(tool::ArrayDesc desc, std::shared_ptr<Desc> base_type)
            : Desc(DescType::array_int), desc(desc), base_type(std::move(base_type)) {}
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
        std::shared_ptr<ast::Program> root;
        Config config;
        ast::tool::Evaluator eval;

        brgen::writer::Writer code;

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
            else if (conf.name == "config.cpp.vector_mode") {
                auto r = tool::get_config_value<tool::EResultType::string>(eval, conf);
                if (!r) {
                    return r.transform(empty_void);
                }
                if (*r == "std_vector") {
                    config.vector_mode = VectorMode::std_vector;
                }
                else if (*r == "pointer") {
                    config.vector_mode = VectorMode::pointer;
                }
                else {
                    return brgen::unexpect(brgen::error(conf.loc, "invalid vector mode"));
                }
            }
            return {};
        }

        auto get_primitive_type(size_t bit_size, bool is_signed) -> std::string_view {
            switch (bit_size) {
                case 8:
                    if (is_signed) {
                        return "std::int8_t";
                    }
                    else {
                        return "std::uint8_t";
                    }
                case 16:
                    if (is_signed) {
                        return "std::int16_t";
                    }
                    else {
                        return "std::uint16_t";
                    }
                case 32:
                    if (is_signed) {
                        return "std::int32_t";
                    }
                    else {
                        return "std::uint32_t";
                    }
                case 64:
                    if (is_signed) {
                        return "std::int64_t";
                    }
                    else {
                        return "std::uint64_t";
                    }
                default:
                    return {};
            }
        }

        brgen::result<void> collect_field(Fields& f, const std::shared_ptr<ast::Format>& fmt, std::shared_ptr<ast::Field>&& field) {
            auto& fname = field->ident->ident;
            if (auto a = tool::is_array_type(field->field_type, eval)) {
                auto b = tool::is_int_type(a->base_type);
                if (!b) {
                    return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
                }
                auto type = get_primitive_type(b->bit_size, b->is_signed);
                if (type.empty()) {
                    return brgen::unexpect(brgen::error(field->loc, "unsupported type"));
                }
                if (a->length_eval) {
                    config.includes.emplace("<array>");
                    f.push(std::make_shared<Field>(field, std::make_shared<IntArrayDesc>(std::move(*a), std::make_shared<IntDesc>(std::move(*b)))));
                }
                else {
                    if (config.vector_mode == VectorMode::std_vector) {
                        config.includes.emplace("<vector>");
                    }
                    auto vec = std::make_shared<VectorDesc>(std::move(*a), std::make_shared<IntDesc>(std::move(*b)));
                    tool::LinerResolver resolver;
                    if (!resolver.resolve(vec->desc.length)) {
                        return brgen::unexpect(brgen::error(field->loc, "cannot resolve vector length"));
                    }
                    if (tool::belong_to(resolver.about) != fmt) {
                        return brgen::unexpect(brgen::error(field->loc, "cannot resolve vector length"));
                    }
                    vec->resolved_expr = std::move(resolver.resolved);
                    auto vec_field = std::make_shared<Field>(field, std::move(vec));
                    bool ok = false;
                    for (auto& f : f.fields) {
                        if (f->base->ident->ident == resolver.about->ident) {
                            if (f->length_related.lock()) {
                                return brgen::unexpect(brgen::error(field->loc, "cannot resolve vector length"));
                            }
                            f->length_related = vec_field;
                            ok = true;
                            break;
                        }
                    }
                    if (!ok) {
                        return brgen::unexpect(brgen::error(field->loc, "cannot resolve vector length"));
                    }
                    f.push(std::move(vec_field));
                }
            }
            else if (auto i = tool::is_int_type(field->field_type)) {
                auto type = get_primitive_type(i->bit_size, i->is_signed);
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
                    if (f.fields[i]->desc->type == DescType::array_int) {
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
                auto& desc = static_cast<IntDesc*>(f.desc.get())->desc;
                code.writeln(get_primitive_type(desc.bit_size, desc.is_signed), " ", f.base->ident->ident, ";");
            }
            else {
                auto arr_desc = static_cast<IntArrayDesc*>(f.desc.get());
                auto& i_desc = static_cast<IntDesc*>(arr_desc->base_type.get())->desc;
                auto& len = arr_desc->desc.length_eval;
                if (len) {
                    code.writeln("std::array<", get_primitive_type(i_desc.bit_size, i_desc.is_signed), ",", brgen::nums(len->get<tool::EResultType::integer>()), "> ", f.base->ident->ident, ";");
                }
                else {
                    code.writeln("std::vector<", get_primitive_type(i_desc.bit_size, i_desc.is_signed), "> ", f.base->ident->ident, ";");
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

        void method_with_error_fn(std::string_view method, std::string_view io_object, auto&& fn) {
            code.write("if(!", method, "(", io_object, ",");
            fn();
            code.writeln(")) {");
            code.indent_writeln("return false;");
            code.writeln("}");
        }

        void method_with_error(std::string_view method, std::string_view io_object, auto&&... args) {
            method_with_error_fn(method, io_object, [&] {
                code.write(args...);
            });
        }

        brgen::result<void> generate_decoder(MergedFields& f) {
            code.writeln("constexpr bool ", "parse", "(::utils::binary::reader& r) {");
            constexpr auto is_be = "true";
            constexpr auto io_object = "r";
            constexpr auto num_method = "::utils::binary::read_num";
            constexpr auto bulk_method = "::utils::binary::read_num_bulk";
            {
                auto sc = code.indent_scope();
                for (auto& field : f.fields) {
                    if (BulkFields* f = std::get_if<BulkFields>(&field)) {
                        method_with_error_fn(bulk_method, io_object, [&](auto&&... args) {
                            code.write(is_be);
                            for (auto& field : f->fields) {
                                if (field->desc->type == DescType::array_int) {
                                    auto len = static_cast<IntArrayDesc*>(field->desc.get())->desc.length_eval->get<tool::EResultType::integer>();
                                    for (size_t i = 0; i < len; i++) {
                                        code.write(",", field->base->ident->ident, "[", brgen::nums(i), "]");
                                    }
                                }
                                else {
                                    code.write(",", field->base->ident->ident);
                                }
                            }
                        });
                    }
                    else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                        auto& f = *fp;
                        if (f->desc->type == DescType::array_int) {
                            auto desc = static_cast<IntArrayDesc*>(f->desc.get());
                            auto len = desc->desc.length_eval->get<tool::EResultType::integer>();
                            method_with_error_fn(num_method, io_object, [&] {
                                code.write(is_be);
                                for (size_t i = 0; i < len; i++) {
                                    code.write(",", f->base->ident->ident, "[", brgen::nums(i), "]");
                                }
                            });
                        }
                        else if (f->desc->type == DescType::vector) {
                            auto desc = static_cast<VectorDesc*>(f->desc.get());
                            tool::Stringer s;
                            s.to_string(desc->desc.length);
                            code.writeln("if(", s.buffer, " > ", f->base->ident->ident, ".max_size()) {");
                            code.indent_writeln("return false;");
                            code.writeln("}");
                            code.writeln(f->base->ident->ident, ".resize(", s.buffer, ");");
                            auto i = "i_" + f->base->ident->ident;
                            code.writeln("for(size_t ", i, " = 0;", i, " < ", f->base->ident->ident, ".size();", i, "++) {");
                            {
                                auto sc = code.indent_scope();
                                method_with_error(num_method, io_object, f->base->ident->ident, "[", i, "]", ",", is_be);
                            }
                            code.writeln("}");
                        }
                        else {
                            method_with_error(num_method, io_object, f->base->ident->ident, ",", is_be);
                        }
                    }
                }
                code.writeln("return true;");
            }
            code.writeln("}");
            return {};
        }

        brgen::result<void> generate_encoder(MergedFields& f) {
            constexpr auto is_be = "true";
            constexpr auto io_object = "w";
            constexpr auto num_method = "::utils::binary::write_num";
            constexpr auto bulk_method = "::utils::binary::write_num_bulk";
            code.writeln("constexpr bool ", "render", "(::utils::binary::writer& w) const {");
            auto write_vec_length_check = [&](auto&& f, auto&& vec) {
                auto vec_desc = static_cast<VectorDesc*>(vec->desc.get());
                tool::Stringer s;
                auto field_name = vec->base->ident->ident + ".size()";
                s.tmp_var_map[0] = field_name;
                s.to_string(vec_desc->resolved_expr);
                auto tmp = "tmp_len_" + f->base->ident->ident;
                code.writeln("auto ", tmp, " = ", s.buffer, ";");
                auto int_desc = static_cast<IntDesc*>(f->desc.get());
                config.includes.emplace("<limits>");
                code.writeln("if(", tmp, " > (std::numeric_limits<", get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed), ">::max)()", ") {");
                code.indent_writeln("return false;");
                code.writeln("}");
                if (field_name != s.buffer) {
                    s.ident_map[f->base->ident->ident] = tmp;
                    s.buffer.clear();
                    s.to_string(vec_desc->desc.length);
                    code.writeln("if(", vec->base->ident->ident, ".size()", " != ", s.buffer, ") {");
                    code.indent_writeln("return false;");
                    code.writeln("}");
                }
            };
            {
                auto sc = code.indent_scope();
                for (auto& field : f.fields) {
                    if (BulkFields* f = std::get_if<BulkFields>(&field)) {
                        for (auto& field : f->fields) {
                            if (auto vec = field->length_related.lock()) {
                                write_vec_length_check(field, vec);
                            }
                        }
                        method_with_error_fn(bulk_method, io_object, [&] {
                            for (auto& field : f->fields) {
                                if (field->desc->type == DescType::array_int) {
                                    auto len = static_cast<IntArrayDesc*>(field->desc.get())->desc.length_eval->get<tool::EResultType::integer>();
                                    for (size_t i = 0; i < len; i++) {
                                        code.write(",", field->base->ident->ident, "[", brgen::nums(i), "]");
                                    }
                                }
                                else {
                                    if (!field->length_related.expired()) {
                                        auto tmp = "tmp_len_" + field->base->ident->ident;
                                        auto int_desc = static_cast<IntDesc*>(field->desc.get());
                                        code.write(",", get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed), "(", tmp, ")");
                                    }
                                    else {
                                        code.write(",", field->base->ident->ident);
                                    }
                                }
                            }
                        });
                    }
                    else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                        auto& f = *fp;
                        if (f->desc->type == DescType::array_int) {
                            auto desc = static_cast<IntArrayDesc*>(f->desc.get());
                            auto len = desc->desc.length_eval->get<tool::EResultType::integer>();
                            method_with_error_fn(bulk_method, io_object, [&] {
                                code.write(is_be);
                                for (size_t i = 0; i < len; i++) {
                                    code.write(",", f->base->ident->ident, "[", brgen::nums(i), "]");
                                }
                            });
                        }
                        else if (f->desc->type == DescType::vector) {
                            auto desc = static_cast<VectorDesc*>(f->desc.get());
                            auto i = "i_" + f->base->ident->ident;
                            code.writeln("for(size_t ", i, " = 0;", i, " < ", f->base->ident->ident, ".size();", i, "++) {");
                            {
                                auto sc = code.indent_scope();
                                method_with_error(num_method, io_object, f->base->ident->ident, "[", i, "]", ",", is_be);
                            }
                            code.writeln("}");
                        }
                        else {
                            if (auto vec = f->length_related.lock()) {
                                write_vec_length_check(f, vec);
                                auto tmp = "tmp_len_" + f->base->ident->ident;
                                auto int_desc = static_cast<IntDesc*>(f->desc.get());
                                method_with_error(num_method, io_object, get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed), "(", tmp, ")", ",", is_be);
                            }
                            else {
                                method_with_error(num_method, io_object, f->base->ident->ident, ",", is_be);
                            }
                        }
                    }
                }
                code.writeln("return true;");
            }
            code.writeln("}");
            return {};
        }

        brgen::result<void> generate_format(const std::shared_ptr<ast::Format>& fmt) {
            auto name = fmt->ident->ident;
            auto& fields = fmt->struct_type->fields;
            Fields fs;
            code.writeln("struct ", name, " {");
            {
                auto sc = code.indent_scope();

                for (auto& f : fields) {
                    if (ast::as<ast::Field>(f)) {
                        if (auto r = collect_field(fs, fmt, ast::cast_to<ast::Field>(f)); !r) {
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

                code.writeln();

                if (mfs.fields.size() > 0) {
                    if (auto r = generate_encoder(mfs); !r) {
                        return r.transform(empty_void);
                    }
                    code.writeln();
                    if (auto r = generate_decoder(mfs); !r) {
                        return r.transform(empty_void);
                    }
                }
            }
            code.writeln("};");
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
            for (auto& fmt : formats) {
                auto r = generate_format(fmt);
                if (!r) {
                    return r;
                }
            }
            auto text = std::move(code.out());

            code.writeln("// Code generated by json2cpp.");
            code.writeln("#pragma once");

            for (auto& inc : config.includes) {
                code.writeln("#include ", inc);
            }

            code.writeln();

            code.write_unformatted(text);

            code.writeln();

            return {};
        }
    };
}  // namespace json2cpp
