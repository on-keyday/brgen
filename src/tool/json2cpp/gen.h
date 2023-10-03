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

    static constexpr auto empty_void = utils::helper::either::empty_value<void>();
    struct Config {
        std::string namespace_;
        std::set<std::string> includes;
        std::set<std::string> exports;
        VectorMode vector_mode = VectorMode::std_vector;

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
            return {};
        }
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

    struct DefineLength {
        std::string field_name;
        std::string length;
    };

    struct AssertFalse {
        std::string comment;
        std::string cond;
    };

    struct ApplyBulk {
        std::vector<std::string> field_names;
    };

    struct ApplyVector {
        std::string field_name;
        std::string length_var;
    };

    struct ApplyInt {
        std::string field_name;
    };

    using Event = std::variant<ApplyInt, DefineLength, AssertFalse, ApplyBulk, ApplyVector>;

    enum class Method {
        encode,
        decode,
    };

    struct Generator {
        std::shared_ptr<ast::Program> root;
        Config config;
        ast::tool::Evaluator eval;

        brgen::writer::Writer code;

        Generator(std::shared_ptr<ast::Program> root)
            : root(std::move(root)) {}

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

        brgen::result<void> collect_vector_field(Fields& f, tool::IntDesc& b, tool::ArrayDesc& a, const std::shared_ptr<ast::Format>& fmt, std::shared_ptr<ast::Field>&& field) {
            if (config.vector_mode == VectorMode::std_vector) {
                config.includes.emplace("<vector>");
            }
            auto vec = std::make_shared<VectorDesc>(std::move(a), std::make_shared<IntDesc>(std::move(b)));
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
                    vec_field->length_related = f;
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                return brgen::unexpect(brgen::error(field->loc, "cannot resolve vector length"));
            }
            f.push(std::move(vec_field));
            return {};
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
                    if (auto res = collect_vector_field(f, *b, *a, fmt, std::move(field)); !res) {
                        return res.transform(empty_void);
                    }
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

        brgen::result<void> add_bulk_array(ApplyBulk& bulk, const std::shared_ptr<Field>& field) {
            auto len = static_cast<IntArrayDesc*>(field->desc.get())->desc.length_eval->get<tool::EResultType::integer>();
            for (auto i = 0; i < len; i++) {
                bulk.field_names.push_back(field->base->ident->ident + "[" + brgen::nums(i) + "]");
            }
            return {};
        }

        brgen::result<std::string> handle_encoder_int_length_related(std::vector<Event>& event, const std::shared_ptr<Field>& field) {
            if (auto vec = field->length_related.lock()) {
                auto desc = static_cast<VectorDesc*>(vec->desc.get());
                tool::Stringer s;
                auto tmp = "tmp_len_" + vec->base->ident->ident;
                auto base = vec->base->ident->ident + ".size()";
                s.tmp_var_map[0] = base;
                s.to_string(desc->resolved_expr);
                event.push_back(DefineLength{
                    .field_name = tmp,
                    .length = std::move(s.buffer),
                });
                auto int_desc = static_cast<IntDesc*>(field->desc.get());
                config.includes.emplace("<limits>");
                event.push_back(AssertFalse{
                    .comment = "check overflow",
                    .cond = brgen::concat(tmp, " > ", "(std::numeric_limits<", std::string(get_primitive_type(int_desc->desc.bit_size, int_desc->desc.is_signed)), ">::max)()"),
                });
                if (s.buffer != base) {
                    s.buffer.clear();
                    s.ident_map[field->base->ident->ident] = tmp;
                    s.to_string(desc->desc.length);
                    event.push_back(AssertFalse{
                        .comment = "check truncated",
                        .cond = brgen::concat(base, " != ", s.buffer),
                    });
                }
                return tmp;
            }
            return field->base->ident->ident;
        }

        brgen::result<void> convert_bulk(std::vector<Event>& event, BulkFields* b, Method method) {
            ApplyBulk bulk;
            for (auto& field : b->fields) {
                if (field->desc->type == DescType::array_int) {
                    if (auto res = add_bulk_array(bulk, field); !res) {
                        return res.transform(empty_void);
                    }
                }
                else {
                    assert(field->desc->type == DescType::int_);
                    if (method == Method::encode) {
                        auto name = handle_encoder_int_length_related(event, field);
                        if (!name) {
                            return name.transform(empty_void);
                        }
                        bulk.field_names.push_back(std::move(*name));
                    }
                    else {
                        bulk.field_names.push_back(field->base->ident->ident);
                    }
                }
            }
            event.push_back(std::move(bulk));
            return {};
        }

        brgen::result<void> convert_vec_decode(std::vector<Event>& event, const std::shared_ptr<Field>& vec) {
            auto vec_desc = static_cast<VectorDesc*>(vec->desc.get());
            auto int_desc = static_cast<IntDesc*>(vec_desc->base_type.get());
            auto len_field = vec->length_related.lock();
            tool::Stringer s;
            auto tmp = "tmp_len_" + vec->base->ident->ident;
            s.to_string(vec_desc->desc.length);
            event.push_back(DefineLength{
                .field_name = tmp,
                .length = s.buffer,
            });
            event.push_back(AssertFalse{
                .comment = "check size for buffer limit",
                .cond = brgen::concat(tmp, " > ", vec->base->ident->ident, ".max_size()"),
            });
            if (tmp != s.buffer) {
                s.buffer.clear();
                s.tmp_var_map[0] = tmp;
                s.to_string(vec_desc->resolved_expr);
                event.push_back(AssertFalse{
                    .comment = "check overflow",
                    .cond = brgen::concat(len_field->base->ident->ident, "!=", s.buffer),
                });
            }
            event.push_back(ApplyVector{
                .field_name = vec->base->ident->ident,
                .length_var = tmp,
            });
            return {};
        }

        brgen::result<void> convert_to_decoder_events(std::vector<Event>& event, MergedFields& f) {
            for (auto& field : f.fields) {
                if (auto f = std::get_if<BulkFields>(&field)) {
                    if (auto res = convert_bulk(event, f, Method::decode); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::int_) {
                        auto int_desc = static_cast<IntDesc*>(f->desc.get());
                        event.push_back(ApplyInt{
                            .field_name = f->base->ident->ident,
                        });
                    }
                    else if (f->desc->type == DescType::array_int) {
                        ApplyBulk bulk;
                        if (auto res = add_bulk_array(bulk, f); !res) {
                            return res.transform(empty_void);
                        }
                        event.push_back(std::move(bulk));
                    }
                    else {
                        if (auto res = convert_vec_decode(event, f); !res) {
                            return res.transform(empty_void);
                        }
                    }
                }
            }
            return {};
        }

        brgen::result<void> convert_to_encoder_events(std::vector<Event>& event, MergedFields& f) {
            size_t tmp_ = 0;
            for (auto& field : f.fields) {
                if (auto f = std::get_if<BulkFields>(&field)) {
                    if (auto res = convert_bulk(event, f, Method::encode); !res) {
                        return res.transform(empty_void);
                    }
                }
                else if (auto fp = std::get_if<std::shared_ptr<Field>>(&field)) {
                    auto& f = *fp;
                    if (f->desc->type == DescType::int_) {
                        auto name = handle_encoder_int_length_related(event, f);
                        if (!name) {
                            return name.transform(empty_void);
                        }
                        auto int_desc = static_cast<IntDesc*>(f->desc.get());
                        event.push_back(ApplyInt{
                            .field_name = std::move(*name),
                        });
                    }
                    else if (f->desc->type == DescType::array_int) {
                        ApplyBulk bulk;
                        if (auto res = add_bulk_array(bulk, f); !res) {
                            return res.transform(empty_void);
                        }
                        event.push_back(std::move(bulk));
                    }
                    else {
                        auto tmp = "tmp_len_" + f->base->ident->ident;
                        event.push_back(ApplyVector{
                            .field_name = f->base->ident->ident,
                            .length_var = tmp,
                        });
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
                    if (config.vector_mode == VectorMode::std_vector) {
                        code.writeln("std::vector<", get_primitive_type(i_desc.bit_size, i_desc.is_signed), "> ", f.base->ident->ident, ";");
                    }
                    else {  // pointer
                        assert(config.vector_mode == VectorMode::pointer);
                        code.writeln(get_primitive_type(i_desc.bit_size, i_desc.is_signed), "* ", f.base->ident->ident, ";");
                    }
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

        brgen::result<void> generate_encoder(MergedFields& f) {
            std::vector<Event> events;
            if (auto res = convert_to_encoder_events(events, f); !res) {
                return res.transform(empty_void);
            }
            code.writeln("constexpr bool ", "render", "(::utils::binary::writer& w) const {");
            constexpr auto is_be = "true";
            constexpr auto io_object = "w";
            constexpr auto num_method = "::utils::binary::write_num";
            constexpr auto bulk_method = "::utils::binary::write_num_bulk";
            auto scope = code.indent_scope();
            for (auto& event : events) {
                if (auto bulk = std::get_if<ApplyBulk>(&event)) {
                    method_with_error_fn(bulk_method, io_object, [&] {
                        code.write(is_be);
                        for (auto& field : bulk->field_names) {
                            code.write(",", field);
                        }
                    });
                }
                else if (auto vec = std::get_if<ApplyVector>(&event)) {
                    auto i = "i_" + vec->field_name;
                    code.writeln("for(size_t ", i, " = 0;", i, "<", vec->length_var, ";", i, "++) {");
                    {
                        auto s = code.indent_scope();
                        method_with_error(num_method, io_object, vec->field_name, "[", i, "]", ",", is_be);
                    }
                    code.writeln("}");
                }
                else if (auto int_ = std::get_if<ApplyInt>(&event)) {
                    method_with_error(num_method, io_object, int_->field_name, ",", is_be);
                }
                else if (auto len = std::get_if<DefineLength>(&event)) {
                    code.writeln("size_t ", len->field_name, " = ", len->length, ";");
                }
                else if (auto cond = std::get_if<AssertFalse>(&event)) {
                    code.writeln("// ", cond->comment);
                    code.writeln("if(", cond->cond, ") {");
                    code.indent_writeln("return false;");
                    code.writeln("}");
                }
            }

            code.writeln("return true;");
            scope.execute();
            code.writeln("}");
            code.writeln();
            return {};
        }

        brgen::result<void> generate_decoder(MergedFields& f) {
            std::vector<Event> events;
            if (auto res = convert_to_decoder_events(events, f); !res) {
                return res.transform(empty_void);
            }
            constexpr auto is_be = "true";
            constexpr auto io_object = "r";
            constexpr auto num_method = "::utils::binary::read_num";
            constexpr auto bulk_method = "::utils::binary::read_num_bulk";
            code.writeln("constexpr bool ", "parse", "(::utils::binary::reader& r) {");
            auto scope = code.indent_scope();  // enter scope
            for (auto& event : events) {
                if (auto bulk = std::get_if<ApplyBulk>(&event)) {
                    if (config.vector_mode == VectorMode::std_vector) {
                        code.writeln();
                    }
                    method_with_error_fn(bulk_method, io_object, [&] {
                        code.write(is_be);
                        for (auto& field : bulk->field_names) {
                            code.write(",", field);
                        }
                    });
                }
                else if (auto vec = std::get_if<ApplyVector>(&event)) {
                    auto i = "i_" + vec->field_name;
                    code.writeln("for(size_t ", i, " = 0;", i, "<", vec->length_var, ";", i, "++) {");
                    {
                        auto s = code.indent_scope();
                        method_with_error(num_method, io_object, vec->field_name, "[", i, "]", ",", is_be);
                    }
                    code.writeln("}");
                }
                else if (auto int_ = std::get_if<ApplyInt>(&event)) {
                    method_with_error(num_method, io_object, int_->field_name, ",", is_be);
                }
                else if (auto len = std::get_if<DefineLength>(&event)) {
                    code.writeln("size_t ", len->field_name, " = ", len->length, ";");
                }
                else if (auto cond = std::get_if<AssertFalse>(&event)) {
                    code.writeln("// ", cond->comment);
                    code.writeln("if(", cond->cond, ") {");
                    code.indent_writeln("return false;");
                    code.writeln("}");
                }
            }
            code.writeln("return true;");
            scope.execute();  // leave scope
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
                    auto r = config.handle_config(eval, conf.value());
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
