/*license*/
#pragma once

#include "../ast.h"
#include "eval.h"

namespace brgen::ast::tool {

    struct ArrayDesc {
        std::shared_ptr<Type> base_type;
        std::shared_ptr<Expr> length;
        EResult length_eval;
    };

    inline std::optional<ArrayDesc> is_array_type(auto&& typ, Evaluator& eval, bool omit_eval = false) {
        if (auto arr = ast::as<ArrayType>(typ)) {
            ArrayDesc desc;
            desc.base_type = arr->base_type;
            desc.length = arr->length;
            if (arr->length && !omit_eval) {
                desc.length_eval = eval.eval_as<EResultType::integer>(arr->length);
            }
            return desc;
        }
        return std::nullopt;
    }

    using IntDesc = ast::IntTypeDesc;

    inline std::optional<IntDesc> is_int_type(auto&& typ) {
        if (auto i = ast::as<IntType>(typ)) {
            IntDesc desc;
            desc.bit_size = i->bit_size;
            desc.is_signed = i->is_signed;
            desc.endian = i->endian;
            return desc;
        }
        return std::nullopt;
    }

    struct UnionField {
        std::shared_ptr<Expr> cond;
        std::shared_ptr<Field> field;
        IntDesc desc;
    };

    struct IntUnionDesc {
        size_t largest_bit_size = 0;
        std::shared_ptr<Match> match;
        std::vector<UnionField> fields;
    };

    inline std::optional<IntUnionDesc> is_int_union_type(auto&& typ) {
        if (ast::UnionType* e = ast::as<ast::UnionType>(typ)) {
            IntUnionDesc desc;
            auto loc = e->base.lock();
            auto m = ast::as<ast::Match>(loc);
            if (!m) {
                return std::nullopt;
            }
            desc.match = ast::cast_to<ast::Match>(loc);
            std::vector<std::shared_ptr<ast::MatchBranch>> branch;
            for (auto& br : desc.match->branch) {
                if (ast::as<ast::MatchBranch>(br)) {
                    branch.push_back(ast::cast_to<ast::MatchBranch>(br));
                }
            }
            if (branch.size() != e->fields.size()) {
                return std::nullopt;
            }
            for (size_t i = 0; i < branch.size(); i++) {
                auto s = e->fields[i];
                if (s->fields.size() != 1) {
                    return std::nullopt;
                }
                auto f = ast::as<Field>(s->fields[0]);
                if (!f) {
                    return std::nullopt;
                }
                auto i_desc = is_int_type(f->field_type);
                if (!i_desc) {
                    return std::nullopt;
                }
                UnionField uf;
                uf.cond = branch[i]->cond;
                uf.field = ast::cast_to<Field>(s->fields[0]);
                uf.desc = *i_desc;
                desc.largest_bit_size = std::max(desc.largest_bit_size, i_desc->bit_size);
                desc.fields.push_back(uf);
            }
            return desc;
        }
        return std::nullopt;
    }

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
}  // namespace brgen::ast::tool
