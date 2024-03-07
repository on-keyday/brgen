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
            desc.base_type = arr->element_type;
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
            desc.bit_size = *i->bit_size;
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
        std::shared_ptr<Expr> base;
        std::shared_ptr<Expr> cond;
        std::vector<UnionField> fields;
        bool ignore_if_not_match = false;
    };

    inline std::optional<IntUnionDesc> is_int_union_type(auto&& typ) {
        if (ast::StructUnionType* e = ast::as<ast::StructUnionType>(typ)) {
            IntUnionDesc desc;
            auto loc = e->base.lock();
            if (auto m = ast::as<ast::Match>(loc)) {
                desc.base = loc;
                desc.cond = m->cond;
                std::vector<std::shared_ptr<ast::MatchBranch>> branch;
                for (auto& br : m->branch) {
                    if (ast::as<ast::MatchBranch>(br)) {
                        branch.push_back(ast::cast_to<ast::MatchBranch>(br));
                    }
                }
                if (branch.size() != e->structs.size()) {
                    return std::nullopt;
                }
                for (size_t i = 0; i < branch.size(); i++) {
                    auto s = e->structs[i];
                    if (s->fields.size() != 1) {
                        return std::nullopt;
                    }
                    auto f = ast::as<Field>(s->fields[0]);
                    if (!f) {
                        return std::nullopt;
                    }
                    auto i_desc = is_int_type(f->field_type);
                    if (!i_desc) {
                        // detect .. => :void
                        if (!desc.ignore_if_not_match &&
                            ast::as<ast::VoidType>(f->field_type)) {
                            if (auto r = ast::as<ast::Range>(branch[i]->cond);
                                ast::is_any_range(r) &&
                                r->op == ast::BinaryOp::range_exclusive) {
                                desc.ignore_if_not_match = true;
                                continue;
                            }
                        }
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
            else if (ast::If* if_ = ast::as<ast::If>(loc)) {
                desc.base = loc;
                size_t i = 0;
                std::shared_ptr<Field> cur_field;
                auto check_field = [&]() -> std::optional<brgen::ast::IntTypeDesc> {
                    if (e->structs.size() <= i) {
                        return std::nullopt;
                    }
                    auto s = e->structs[i];
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
                    cur_field = ast::cast_to<Field>(s->fields[0]);
                    return i_desc;
                };
                while (if_) {
                    auto i_desc = check_field();
                    if (!i_desc) {
                        return std::nullopt;
                    }
                    UnionField uf;
                    uf.cond = if_->cond;
                    uf.field = std::move(cur_field);
                    uf.desc = *i_desc;
                    desc.largest_bit_size = std::max(desc.largest_bit_size, i_desc->bit_size);
                    desc.fields.push_back(std::move(uf));
                    i++;
                    auto next = ast::as<ast::If>(if_->els);
                    if (next) {
                        if_ = next;
                    }
                    break;
                }
                if (if_->els) {
                    auto i_desc = check_field();
                    if (!i_desc) {
                        return std::nullopt;
                    }
                    UnionField uf;
                    uf.cond = nullptr;
                    uf.field = std::move(cur_field);
                    uf.desc = *i_desc;
                    desc.largest_bit_size = std::max(desc.largest_bit_size, i_desc->bit_size);
                    desc.fields.push_back(std::move(uf));
                }
                desc.ignore_if_not_match = true;
                return desc;
            }
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

    std::shared_ptr<Ident> get_definition(const std::shared_ptr<Ident>& ident) {
        if (!ident) {
            return nullptr;
        }
        auto cur = ident;
        for (;;) {
            auto base = cur->base.lock();
            if (!base) {
                return cur;  // maybe unknown
            }
            if (auto ident = ast::as<ast::Ident>(base)) {
                cur = ast::cast_to<ast::Ident>(base);
            }
            else {
                return cur;
            }
        }
    }
}  // namespace brgen::ast::tool
