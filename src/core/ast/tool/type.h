/*license*/
#pragma once

#include "../ast.h"
#include "eval.h"

namespace brgen::ast::tool {

    inline std::shared_ptr<Format> belong_format(auto&& typ) {
        if (Field* f = ast::as<Field>(typ)) {
            auto fmt = f->belong.lock();
            if (!ast::as<Format>(fmt)) {
                return nullptr;
            }
            return ast::cast_to<Format>(fmt);
        }
        if (Ident* ident = ast::as<Ident>(typ)) {
            return belong_format(ident->base.lock());
        }
        return nullptr;
    }

    inline std::shared_ptr<Field> linked_field(auto&& typ) {
        if (Field* f = ast::as<Field>(typ)) {
            return ast::cast_to<Field>(typ);
        }
        if (Ident* ident = ast::as<Ident>(typ)) {
            return linked_field(ident->base.lock());
        }
        return nullptr;
    }

    bool is_on_named_struct(const std::shared_ptr<ast::Field>& field) {
        if (!field) {
            return false;
        }
        if (auto fmt = ast::as<ast::Format>(field->belong.lock())) {
            return field->belong_struct.lock() == fmt->body->struct_type;
        }
        if (auto state = ast::as<ast::State>(field->belong.lock())) {
            return field->belong_struct.lock() == state->body->struct_type;
        }
        return false;
    }
}  // namespace brgen::ast::tool
