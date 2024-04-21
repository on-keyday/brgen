/*license*/
#pragma once
#include <core/ast/ast.h>

namespace brgen::ast::tool {
    void set_tmp_field_ident(size_t seq, const std::shared_ptr<Field>& f, const char* name_prefix = "tmp") {
        if (f->ident) return;
        auto ident = std::make_shared<Ident>(f->loc, brgen::concat(name_prefix, brgen::nums(seq)));
        ident->base = f;
        f->ident = ident;
        f->ident->usage = IdentUsage::define_field;
        f->ident->constant_level = ConstantLevel::variable;
    }
}  // namespace brgen::ast::tool
