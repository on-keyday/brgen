/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/common/error.h>

namespace brgen::ast::tool {
    bool equal_type(const std::shared_ptr<ast::Type>& left, const std::shared_ptr<ast::Type>& right) {
        // unwrap ident type
        if (auto l = ast::as<ast::IdentType>(left), r = ast::as<ast::IdentType>(right); l || r) {
            if (l && r) {
                return equal_type(l->base.lock(), r->base.lock());
            }
            if (l) {
                return equal_type(l->base.lock(), right);
            }
            return equal_type(left, r->base.lock());
        }
        if (!left || !right || left->node_type != right->node_type) {
            return false;
        }
        if (auto lty = ast::as<ast::IntType>(left)) {
            auto rty = ast::as<ast::IntType>(right);
            return lty->bit_size == rty->bit_size;
        }

        if (auto lty = ast::as<ast::FloatType>(left)) {
            auto rty = ast::as<ast::FloatType>(right);
            return lty->bit_size == rty->bit_size;
        }

        if (auto lty = ast::as<ast::EnumType>(left)) {
            auto rty = ast::as<ast::EnumType>(right);
            return lty->base.lock() == rty->base.lock();
        }
        if (ast::as<ast::BoolType>(left)) {
            return true;
        }
        if (auto lty = ast::as<ast::ArrayType>(left)) {
            auto rty = ast::as<ast::ArrayType>(right);
            if (!equal_type(lty->element_type, rty->element_type)) {
                return false;
            }
            if (lty->length_value && rty->length_value) {
                return *lty->length_value == *rty->length_value;  // static array
            }
            if (lty->length_value || rty->length_value) {
                return false;  // dynamic and static array is not equal
            }
            return true;  // dynamic array is always equal
        }
        if (auto s = ast::as<ast::StructType>(left)) {
            return left == right;  // struct type has same pointer if it is same struct
        }
        if (auto r = ast::as<ast::RangeType>(right)) {
            auto l = ast::as<ast::RangeType>(left);
            if (r->base_type && l->base_type) {
                return equal_type(l->base_type, r->base_type);
            }
            return !r->base_type && !l->base_type;
        }
        return false;
    }

    std::shared_ptr<ast::Type> int_literal_to_int_type(const std::shared_ptr<ast::Type>& base) {
        if (auto ty = ast::as<ast::IntLiteralType>(base)) {
            auto aligned = ty->get_aligned_bit();
            return std::make_shared<ast::IntType>(ty->loc, aligned, ast::Endian::unspec, false);
        }
        return base;
    }

    // int_type_fitting convert IntLiteralType to IntType to assign or compare
    brgen::result<void> int_type_fitting(std::shared_ptr<ast::Type>& left, std::shared_ptr<ast::Type>& right) {
        auto fitting = [&](auto& a, auto& b) -> brgen::result<void> {
            auto ity = ast::as<ast::IntType>(a);
            auto lty = ast::as<ast::IntLiteralType>(b);
            auto bit_size = lty->get_bit_size();
            if (ity->bit_size < *bit_size) {
                // error(lty->loc, "bit size ", nums(*bit_size), " is too large")
                //     .error(ity->loc, "for this")
                //     .report();
                return unexpect(error(lty->loc, "bit size ", nums(*bit_size), " is too large").error(ity->loc, "for this"));
            }
            b = a;  // fitting
            return {};
        };
        if (left->node_type == ast::NodeType::int_type &&
            right->node_type == ast::NodeType::int_literal_type) {
            return fitting(left, right);
        }
        else if (left->node_type == ast::NodeType::int_literal_type &&
                 right->node_type == ast::NodeType::int_type) {
            return fitting(right, left);
        }
        else if (left->node_type == ast::NodeType::int_literal_type &&
                 right->node_type == ast::NodeType::int_literal_type) {
            left = int_literal_to_int_type(left);
            right = int_literal_to_int_type(right);
            auto li = ast::as<ast::IntType>(left);
            auto ri = ast::as<ast::IntType>(right);
            if (li->bit_size > ri->bit_size) {
                right = left;
            }
            else {
                left = right;
            }
        }
        return {};
    }

    brgen::result<bool> comparable_type(std::shared_ptr<ast::Type>& left, std::shared_ptr<ast::Type>& right) {
        if (equal_type(left, right)) {
            return true;
        }
        auto check_range_compare = [&](ast::RangeType* rty, std::shared_ptr<ast::Type>& other_hand) -> brgen::result<bool> {
            if (!rty->base_type) {
                return {};  // range .. or ..= is always comparable to any type
            }
            if (auto ok = int_type_fitting(rty->base_type, other_hand); !ok) {
                return ok.transform([](auto&&...) { return false; });
            }
            return equal_type(rty->base_type, other_hand);
        };
        if (auto rty = ast::as<ast::RangeType>(left)) {
            return check_range_compare(rty, right);
        }
        if (auto rty = ast::as<ast::RangeType>(right)) {
            return check_range_compare(rty, left);
        }
        auto check_array_str_compare = [&](ast::ArrayType* arr, const std::shared_ptr<ast::Type>& other_hand) {
            auto ty = ast::as<ast::IntType>(arr->element_type);
            if (!ty || ty->bit_size != 8) {
                return false;  // only byte array is comparable with string
            }
            if (auto str = ast::as<ast::StrLiteralType>(other_hand)) {
                if (arr->length && arr->length_value) {
                    if (*arr->length_value != str->base.lock()->length) {
                        return false;
                    }
                }
                return true;
            }
            if (auto regex = ast::as<ast::RegexLiteralType>(other_hand)) {
                return true;
            }
            return false;
        };
        if (auto arr = ast::as<ast::ArrayType>(right)) {
            return check_array_str_compare(arr, left);
        }
        if (auto arr = ast::as<ast::ArrayType>(left)) {
            return check_array_str_compare(arr, right);
        }
        if (auto regex = ast::as<ast::RegexLiteralType>(left)) {
            return ast::as<ast::StrLiteralType>(right) || ast::as<ast::ArrayType>(right);
        }
        if (auto regex = ast::as<ast::RegexLiteralType>(right)) {
            return ast::as<ast::StrLiteralType>(left) || ast::as<ast::ArrayType>(left);
        }
        return false;
    }

    brgen::result<std::shared_ptr<ast::Type>> common_type(std::shared_ptr<ast::Type>& a, std::shared_ptr<ast::Type>& b) {
        if (auto ok = int_type_fitting(a, b); !ok) {
            return ok.transform([](auto&&...) { return nullptr; });
        }
        if (equal_type(a, b)) {
            return a;
        }
        if (a->node_type == ast::NodeType::int_type && b->node_type == ast::NodeType::int_type) {
            auto a_i = ast::as<ast::IntType>(a);
            auto b_i = ast::as<ast::IntType>(b);
            if (a_i->bit_size == b_i->bit_size) {
                if (a_i->is_signed == b_i->is_signed) {
                    return a;
                }
                return a_i->is_signed ? b : a;
            }
            if (a_i->bit_size > b_i->bit_size) {
                return a;
            }
            return b;
        }
        if (auto a_u = ast::as<ast::UnionType>(a)) {
            if (!a_u->common_type) {
                return nullptr;
            }
            return common_type(a_u->common_type, b);
        }
        if (auto b_u = ast::as<ast::UnionType>(b)) {
            if (!b_u->common_type) {
                return nullptr;
            }
            return common_type(a, b_u->common_type);
        }
        if (auto a_c = ast::as<ast::StructType>(a)) {
            auto fmt = ast::as<ast::Format>(a_c->base.lock());
            if (fmt) {
                for (auto& fn : fmt->cast_fns) {
                    auto c = fn.lock();
                    // return type cannot be int literal type
                    auto typ = common_type(c->return_type, b);
                    if (typ) {
                        return typ;
                    }
                }
            }
        }
        if (auto b_c = ast::as<ast::StructType>(b)) {
            auto fmt = ast::as<ast::Format>(b_c->base.lock());
            if (fmt) {
                for (auto& fn : fmt->cast_fns) {
                    auto c = fn.lock();
                    // return type cannot be int literal type
                    auto typ = common_type(c->return_type, a);
                    if (typ) {
                        return typ;
                    }
                }
            }
        }
        if (auto a_a = ast::as<ast::ArrayType>(a)) {
            if (auto b_a = ast::as<ast::ArrayType>(b)) {
                if (equal_type(a_a->element_type, b_a->element_type)) {
                    auto base_typ = a_a->element_type;
                    return std::make_shared<ast::ArrayType>(a_a->loc, nullptr, a_a->end_loc, std::move(base_typ));
                }
            }
        }
        return nullptr;
    }

    // OrCond_common_type is used for OrCond type inference
    // this function uses common_type first
    // and also infer common type between range and other types based on RangeType.base_type
    brgen::result<std::shared_ptr<ast::Type>> OrCond_common_type(std::shared_ptr<ast::Type>& a, std::shared_ptr<ast::Type>& b) {
        // special edge case for int literal type
        if (auto t1 = ast::as<ast::IntLiteralType>(a), t2 = ast::as<ast::IntLiteralType>(b); t1 && t2) {
            if (t1->bit_size == t2->bit_size) {
                return a;
            }
            if (t1->bit_size > t2->bit_size) {
                return a;
            }
            return b;
        }
        auto t = common_type(a, b);
        if (!t) {
            return t;
        }
        if (*t) {
            return t;
        }
        if (auto r = ast::as<ast::RangeType>(a)) {
            if (r->base_type) {
                return common_type(r->base_type, b);
            }
        }
        if (auto r = ast::as<ast::RangeType>(b)) {
            if (r->base_type) {
                return common_type(a, r->base_type);
            }
        }
        return nullptr;
    }
}  // namespace brgen::ast::tool