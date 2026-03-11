/*license*/
#include "stub/structs.hpp"
#include <code/code_writer.h>

namespace ebmcodegen {

    std::string write_convert_from_json(const Struct& s) {
        CodeWriter w;
        w.writeln("bool from_json(", s.name, "& obj, const futils::json::JSON& j) {");
        {
            auto scope = w.indent_scope();
            if (s.is_any_ref) {
                w.writeln("std::uint64_t id;");
                w.writeln("if(!futils::json::convert_from_json(j, id)) {");
                {
                    auto if_scope = w.indent_scope();
                    w.writeln("return false;");
                }
                w.writeln("}");
                w.writeln("obj = ", s.name, "{id};");
            }
            else {
                for (auto& field : s.fields) {
                    auto type = std::string(field.type);
                    if (field.attr & TypeAttribute::ARRAY) {
                        type = "std::vector<" + type + ">";
                    }
                    w.writeln("if (auto got = j.at(\"", field.name, "\")) {");
                    {
                        auto scope = w.indent_scope();
                        if (field.attr & TypeAttribute::PTR || field.attr & TypeAttribute::RVALUE) {
                            w.writeln(type, " tmp;");
                            w.writeln("if(!futils::json::convert_from_json(*got, tmp)) {");
                            w.indent_writeln("return false;");
                            w.writeln("}");
                            if (!(field.attr & TypeAttribute::PTR) && type == "bool") {
                                w.writeln("obj.", field.name, "(std::move(tmp));");
                            }
                            else {
                                w.writeln("if(!obj.", field.name, "(std::move(tmp))) {");
                                w.indent_writeln("return false;");
                                w.writeln("}");
                            }
                        }
                        else {
                            w.writeln("if(!futils::json::convert_from_json(*got, obj.", field.name, ")) {");
                            w.indent_writeln("return false;");
                            w.writeln("}");
                        }
                    }
                    w.writeln("}");
                    if (!(field.attr & TypeAttribute::PTR)) {
                        w.writeln("else {");
                        w.indent_writeln("return false;");
                        w.writeln("}");
                    }
                }
            }
            w.writeln("return true;");
        }
        w.writeln("}");
        return w.out();
    }

    std::string write_convert_from_json(const Enum& e) {
        CodeWriter w;
        w.writeln("bool from_json(", e.name, "& obj, const futils::json::JSON& j) {");
        {
            auto scope = w.indent_scope();
            w.writeln("if (auto got = j.get_holder().as_str()) {");
            {
                auto scope = w.indent_scope();
                w.writeln("auto& s = *got;");
                for (auto& member : e.members) {
                    w.writeln("if (s == \"", member.name, "\") {");
                    {
                        auto scope = w.indent_scope();
                        w.writeln("obj = ", e.name, "::", member.name, ";");
                        w.writeln("return true;");
                    }
                    w.writeln("}");
                }
                w.writeln("return false;");
            }
            w.writeln("}");
            w.writeln("return false;");
        }
        w.writeln("}");
        return w.out();
    }
}  // namespace ebmcodegen
