/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/writer/section.h>
#include <core/common/expected.h>
#include <helper/transform.h>
#include <set>
#include <binary/writer.h>
#include "event.h"

namespace json2cpp {

    struct Generator {
        std::shared_ptr<ast::Program> root;
        Config config;
        ast::tool::Evaluator eval;

        brgen::writer::Writer code;

        Generator(std::shared_ptr<ast::Program> root)
            : root(std::move(root)) {}

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

        brgen::result<void> generate_bit_field(const BitFields& f) {
            config.includes.emplace("<binary/flags.h>");
            auto prim = get_primitive_type(ast::aligned_bit(f.fixed_size), false);
            assert(prim.size());
            code.write("::utils::binary::flags_t<", prim);
            std::string base_name = "flags";
            for (auto& field : f.fields) {
                auto i_desc = static_cast<IntDesc*>(field->desc.get());
                code.write(",", brgen::nums(i_desc->desc.bit_size));
                base_name += "_" + field->base->ident->ident;
            }
            code.writeln("> ", base_name, ";");
            for (size_t i = 0; i < f.fields.size(); i++) {
                code.writeln("bits_flag_alias_method(", base_name, ",", brgen::nums(i), ",", f.fields[i]->base->ident->ident, ");");
            }
            return {};
        }

        // generate field
        brgen::result<void> generate_fields(const MergedFields& fields) {
            for (auto& field : fields) {
                if (auto f = std::get_if<BulkFields>(&field)) {
                    for (auto& field : f->fields) {
                        if (auto res = generate_field(*field); !res) {
                            return res.transform(empty_void);
                        }
                    }
                }
                else if (auto f = std::get_if<BitFields>(&field)) {
                    if (auto res = generate_bit_field(*f); !res) {
                        return res.transform(empty_void);
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
            EventConverter e{config};
            if (auto res = e.convert_to_encoder_event(f); !res) {
                return res.transform(empty_void);
            }
            code.writeln("constexpr bool ", "render", "(::utils::binary::writer& w) const {");
            constexpr auto is_be = "true";
            constexpr auto io_object = "w";
            constexpr auto num_method = "::utils::binary::write_num";
            constexpr auto bulk_method = "::utils::binary::write_num_bulk";
            auto scope = code.indent_scope();
            for (auto& event : e.events) {
                if (auto bulk = std::get_if<ApplyBulkInt>(&event)) {
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
                else if (auto bits = std::get_if<ApplyBits>(&event)) {
                    method_with_error(num_method, io_object, bits->base_name, ".as_value()");
                }
                else {
                    return brgen::unexpect(brgen::error({}, "unknown event"));
                }
            }

            code.writeln("return true;");
            scope.execute();
            code.writeln("}");
            code.writeln();
            return {};
        }

        brgen::result<void> generate_decoder(MergedFields& f) {
            EventConverter e{config};
            if (auto res = e.convert_to_decoder_event(f); !res) {
                return res.transform(empty_void);
            }
            constexpr auto is_be = "true";
            constexpr auto io_object = "r";
            constexpr auto num_method = "::utils::binary::read_num";
            constexpr auto bulk_method = "::utils::binary::read_num_bulk";
            code.writeln("constexpr bool ", "parse", "(::utils::binary::reader& r) {");
            auto scope = code.indent_scope();  // enter scope
            for (auto& event : e.events) {
                if (auto bulk = std::get_if<ApplyBulkInt>(&event)) {
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
                else if (auto vec = std::get_if<AllocateVector>(&event)) {
                    if (config.vector_mode == VectorMode::std_vector) {
                        code.writeln(vec->field_name, ".resize(", vec->length_var, ");");
                    }
                    else {
                        assert(config.vector_mode == VectorMode::pointer);
                        code.writeln(vec->field_name, " = new ", vec->element_type, "[", vec->length_var, "];");
                    }
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
                else if (auto bits = std::get_if<ApplyBits>(&event)) {
                    method_with_error(num_method, io_object, bits->base_name, ".as_value()");
                }
                else {
                    return brgen::unexpect(brgen::error({}, "unknown event"));
                }
            }
            code.writeln("return true;");
            scope.execute();  // leave scope
            code.writeln("}");
            return {};
        }

        brgen::result<void> generate_constructor_and_assign(const std::shared_ptr<ast::Format>& fmt, MergedFields& m) {
            // write default constructor, move constructor, move assign
            // to move, we need to set the pointer of source to nullptr

            EventConverter e{config};
            if (auto res = e.convert_to_move_event(m); !res) {
                return res.transform(empty_void);
            }
            code.writeln("constexpr ", fmt->ident->ident, "() noexcept = default;");

            auto write_move = [&] {
                for (auto& field : e.events) {
                    if (auto vec = std::get_if<ApplyVector>(&field)) {
                        code.writeln(vec->field_name, " = other____.", vec->field_name, ";");
                        code.writeln("other____.", vec->field_name, " = nullptr;");
                    }
                    else if (auto bulk = std::get_if<ApplyBulkInt>(&field)) {
                        for (auto& field : bulk->field_names) {
                            code.writeln(field, " = other____.", field, ";");
                        }
                    }
                    else if (auto int_ = std::get_if<ApplyInt>(&field)) {
                        code.writeln(int_->field_name, " = other____.", int_->field_name, ";");
                    }
                }
            };

            code.writeln("constexpr ", fmt->ident->ident, "(", fmt->ident->ident, "&& other____) noexcept {");
            {
                auto s = code.indent_scope();
                write_move();
            }
            code.writeln("}");
            code.writeln();

            code.writeln("constexpr ", fmt->ident->ident, "& operator=(", fmt->ident->ident, "&& other____) noexcept {");
            {
                auto s = code.indent_scope();
                code.writeln("if(this == &other____) {");
                code.indent_writeln("return *this;");
                code.writeln("}");
                code.writeln("free();");
                write_move();
                code.writeln("return *this;");
            }
            code.writeln("}");
            code.writeln();

            return {};
        }

        brgen::result<void> generate_deallocate(const std::shared_ptr<ast::Format>& fmt, MergedFields& m) {
            EventConverter e{config};
            if (auto res = e.convert_to_deallocate_event(m); !res) {
                return res.transform(empty_void);
            }
            code.writeln("constexpr void free() {");
            {
                auto s = code.indent_scope();
                for (auto& field : e.events) {
                    if (auto vec = std::get_if<ApplyVector>(&field)) {
                        code.writeln("delete[] ", vec->field_name, ";");
                        code.writeln(vec->field_name, " = nullptr;");
                    }
                }
            }
            code.writeln("}");
            code.writeln();
            return {};
        }

        brgen::result<void> generate_format(const std::shared_ptr<ast::Format>& fmt) {
            FieldCollector c{config, eval};
            code.writeln("struct ", fmt->ident->ident, " {");
            {
                auto sc = code.indent_scope();

                if (auto r = c.collect(fmt); !r) {
                    return r.transform(empty_void);
                }

                if (auto r = generate_fields(c.m); !r) {
                    return r.transform(empty_void);
                }

                code.writeln();

                if (auto r = generate_encoder(c.m); !r) {
                    return r.transform(empty_void);
                }
                code.writeln();
                if (auto r = generate_decoder(c.m); !r) {
                    return r.transform(empty_void);
                }

                code.writeln();

                if (config.vector_mode == VectorMode::pointer) {
                    if (auto r = generate_deallocate(fmt, c.m); !r) {
                        return r.transform(empty_void);
                    }

                    if (auto r = generate_constructor_and_assign(fmt, c.m); !r) {
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
