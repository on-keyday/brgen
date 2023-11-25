/*license*/
#pragma once

#include "cpp_type.h"
#include <helper/expected.h>
#include <ast/ast.h>

namespace j2cp2 {
    struct Generator {
        void write_simple_struct(std::shared_ptr<ast::Format>& fmt) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }
            std::vector<std::shared_ptr<ast::Field>> fields;
            for (auto& field : fmt->body->struct_type->fields) {
                if (auto f = ast::as<ast::Field>(field); f) {
                    fields.push_back(ast::cast_to<ast::Field>(field));
                }
            }
        }
    };
}  // namespace j2cp2
