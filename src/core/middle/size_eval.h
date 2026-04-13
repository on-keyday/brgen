/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/tool/eval.h>
#include <memory>
#include <optional>
#include <set>

namespace brgen::middle {

    inline std::optional<size_t> try_compute_bit_size(const std::shared_ptr<ast::Type>& type, std::set<ast::Type*>& visited) {
        if (!type) return std::nullopt;
        if (type->bit_size) return type->bit_size;
        if (!visited.insert(type.get()).second) return std::nullopt;
        if (auto ident = ast::as<ast::IdentType>(type)) {
            return try_compute_bit_size(ident->base.lock(), visited);
        }
        if (auto st = ast::as<ast::StructType>(type)) {
            size_t total = 0;
            for (auto& m : st->fields) {
                auto f = ast::as<ast::Field>(m);
                if (!f || !f->field_type) return std::nullopt;
                auto sz = try_compute_bit_size(f->field_type, visited);
                if (!sz) return std::nullopt;
                total += *sz;
            }
            return total;
        }
        if (auto arr = ast::as<ast::ArrayType>(type)) {
            if (!arr->length_value || !arr->element_type) return std::nullopt;
            auto elem_sz = try_compute_bit_size(arr->element_type, visited);
            if (!elem_sz) return std::nullopt;
            return *arr->length_value * *elem_sz;
        }
        return std::nullopt;
    }

    inline std::optional<size_t> try_compute_bit_size(const std::shared_ptr<ast::Type>& type) {
        std::set<ast::Type*> visited;
        return try_compute_bit_size(type, visited);
    }

    inline std::optional<size_t> try_compute_byte_size(const std::shared_ptr<ast::Type>& type) {
        auto bit_sz = try_compute_bit_size(type);
        if (!bit_sz || *bit_sz % 8 != 0) return std::nullopt;
        return *bit_sz / 8;
    }

    // TypeLiteral is unwrapped because its expr_type is meta_type with no
    // bit_size — reading through expr_type would always fail.
    inline bool evaluate_sizeof_node(ast::SizeOf* s) {
        if (!s || s->evaluated_value || !s->target) return false;
        std::shared_ptr<ast::Type> target_type;
        if (auto type_lit = ast::as<ast::TypeLiteral>(s->target)) {
            target_type = type_lit->type_literal;
        }
        else {
            target_type = s->target->expr_type;
        }
        auto byte_size = try_compute_byte_size(target_type);
        if (!byte_size) return false;
        s->evaluated_value = *byte_size;
        s->constant_level = ast::ConstantLevel::constant;
        return true;
    }

    inline bool evaluate_array_length(ast::ArrayType* arr_type) {
        if (!arr_type || arr_type->length_value || !arr_type->length) return false;
        if (arr_type->length->constant_level != ast::ConstantLevel::constant) return false;
        ast::tool::Evaluator eval;
        eval.ident_mode = ast::tool::EvalIdentMode::resolve_ident;
        if (auto val = eval.eval_as<ast::tool::EResultType::integer>(arr_type->length)) {
            arr_type->length_value = val->template get<ast::tool::EResultType::integer>();
            return true;
        }
        return false;
    }

}  // namespace brgen::middle
