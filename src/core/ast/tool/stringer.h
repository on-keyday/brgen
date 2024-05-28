/*license*/
#pragma once
#include <string>
#include "../ast.h"
#include "../traverse.h"
#include <functional>
#include <unordered_map>
#include <env/env.h>
#include "ident.h"

namespace brgen::ast::tool {

    struct Stringer {
        using BinaryFunc = std::function<std::string(Stringer& s, const std::shared_ptr<Binary>& v)>;
        BinaryFunc bin_op_func;
        std::unordered_map<BinaryOp, BinaryFunc> bin_op_map;
        std::unordered_map<UnaryOp, std::function<std::string(Stringer& s, const std::shared_ptr<Expr>&)>> unary_op_map;
        std::unordered_map<size_t, std::string> tmp_var_map;

       private:
        std::unordered_map<std::shared_ptr<ast::Ident>, std::string> def_base_map;

       public:
        std::function<std::string(std::string cond, std::string then, std::string els)> cond_op;
        std::unordered_map<std::string, std::function<std::string(Stringer& s, const std::string& name, const std::vector<std::shared_ptr<ast::Expr>>&)>> call_handler;
        std::function<std::string(Stringer&, const std::shared_ptr<Type>&)> type_resolver;
        std::function<std::string(Stringer&, const std::shared_ptr<IOOperation>&)> io_op_handler;
        std::function<std::string(Stringer&, const std::shared_ptr<Available>&)> available_handler;
        std::function<std::string(Stringer&, const std::shared_ptr<ast::Cast>&)> cast_handler;
        std::string this_access = "(*this)";

        void clear() {
            bin_op_map.clear();
            tmp_var_map.clear();
            def_base_map.clear();
            cond_op = nullptr;
            call_handler.clear();
            type_resolver = nullptr;
        }

        void map_ident(const std::shared_ptr<ast::Ident>& ident, auto&&... text) {
            auto l = lookup_base(ident);
            if (!l) {
                assert(false);
                return;
            }
            def_base_map[l->first] = brgen::concat(text...);
        }

        std::string ident_to_string(const std::shared_ptr<ast::Ident>& ident, const std::string& default_text = "") {
            if (!ident) {
                return "";
            }

            auto d = lookup_base(ident);
            if (!d) {
                return ident->ident;
            }
            auto text = def_base_map[d->first];
            auto get_text_with_replace = [&](auto&& replace) {
                if (text.find("${THIS}") != std::string::npos || text.find("$THIS") != std::string::npos) {
                    text = futils::env::expand<std::string>(text, [&](auto&& out, auto&& read) {
                        std::string s;
                        if (!read(s)) {
                            return;
                        }
                        assert(s == "THIS");
                        futils::strutil::append(out, replace);
                    });
                }
                return text;
            };
            if (!d->second) {  // not via member access
                if (ast::as<ast::Member>(d->first->base.lock())) {
                    return get_text_with_replace(this_access);
                }
            }
            if (ast::as<ast::EnumMember>(d->first->base.lock())) {
                return text;
            }
            return get_text_with_replace(default_text);
        }

        std::string default_bin_op(const std::shared_ptr<Binary>& bin) {
            auto left = to_string(bin->left);
            auto right = to_string(bin->right);
            return concat("(", left, " ", ast::to_string(bin->op), " ", right, ")");
        }

        std::string bin_op_with_lookup(const std::shared_ptr<Binary>& bin) {
            if (auto it = bin_op_map.find(bin->op); it != bin_op_map.end()) {
                return it->second(*this, bin);
            }
            else {
                return default_bin_op(bin);
            }
        }

       private:
        std::string handle_bin_op(const std::shared_ptr<Binary>& bin) {
            if (bin_op_func) {
                return bin_op_func(*this, bin);
            }
            return bin_op_with_lookup(bin);
        }

        std::string handle_call_op(const std::shared_ptr<Call>& c) {
            auto callee = to_string(c->callee);
            if (auto found = call_handler.find(callee); found != call_handler.end()) {
                return found->second(*this, callee, c->arguments);
            }
            else {
                std::string args;
                for (auto&& arg : c->arguments) {
                    if (!args.empty()) {
                        args += ", ";
                    }
                    args += to_string(arg);
                }
                return concat(callee, "(", args, ")");
            }
        }

       public:
        std::string to_string(const std::shared_ptr<Expr>& expr) {
            if (!expr) {
                return "";
            }
            if (auto e = ast::as<ast::Binary>(expr)) {
                return handle_bin_op(ast::cast_to<ast::Binary>(expr));
            }
            if (auto lit = ast::as<ast::IntLiteral>(expr)) {
                return lit->value;
            }
            if (auto id = ast::as<ast::Ident>(expr)) {
                return ident_to_string(ast::cast_to<ast::Ident>(expr));
            }
            if (auto tmp = ast::as<ast::TmpVar>(expr)) {
                if (auto found = tmp_var_map.find(tmp->tmp_var); found != tmp_var_map.end()) {
                    return found->second;
                }
                else {
                    return concat("tmp", nums(tmp->tmp_var));
                }
            }
            if (auto c = ast::as<ast::Cond>(expr)) {
                if (cond_op) {
                    return cond_op(to_string(c->cond), to_string(c->then), to_string(c->els));
                }
                else {
                    return concat("(", to_string(c->cond), " ? ", to_string(c->then), " : ", to_string(c->els), ")");
                }
            }
            if (auto c = ast::as<ast::Call>(expr)) {
                return handle_call_op(ast::cast_to<ast::Call>(expr));
            }
            if (auto d = ast::as<ast::Unary>(expr)) {
                return concat(ast::to_string(d->op), to_string(d->expr));
            }
            if (auto a = ast::as<ast::Available>(expr)) {
                if (available_handler) {
                    return available_handler(*this, ast::cast_to<ast::Available>(expr));
                }
                auto ident = ast::as<ast::Ident>(a->target);
                if (!ident) {
                    return "false";  // TODO(on-keyday): support member access
                }
                auto base_ident = ast::as<ast::Ident>(ident->base.lock());
                if (!base_ident) {
                    return "false";
                }
                if (auto field = ast::as<ast::Field>(base_ident->base.lock())) {
                    if (auto typ = ast::as<ast::UnionType>(field->field_type)) {
                        std::string cond_s = "";
                        if (auto c = typ->cond.lock()) {
                            cond_s = to_string(c);
                        }
                        else {
                            cond_s = "true";
                        }
                        std::string result;
                        for (auto& c : typ->candidates) {
                            auto expr = c->cond.lock();
                            auto s = to_string(expr);
                            result += brgen::concat("(", s, "==", cond_s, ") ? ");
                            if (c->field.lock()) {
                                result += "true";
                            }
                            else {
                                result += "false";
                            }
                            result += " : ";
                        }
                        result += "false";
                        return brgen::concat("(", result, ")");
                    }
                    return "true";
                }
            }
            if (auto cast_ = ast::as<ast::Cast>(expr)) {
                if (cast_handler) {
                    return cast_handler(*this, ast::cast_to<ast::Cast>(expr));
                }
                return concat(type_resolver(*this, cast_->expr_type), "(", to_string(cast_->expr), ")");
            }
            if (auto access = ast::as<ast::MemberAccess>(expr)) {
                auto base = to_string(access->target);
                return ident_to_string(access->member, base);
            }
            if (auto io_op = ast::as<ast::IOOperation>(expr)) {
                if (io_op->method == ast::IOMethod::config_endian_big ||
                    io_op->method == ast::IOMethod::config_endian_native) {
                    return "0";
                }
                if (io_op->method == ast::IOMethod::config_endian_little) {
                    return "1";
                }
                if (io_op_handler) {
                    return io_op_handler(*this, ast::cast_to<ast::IOOperation>(expr));
                }
            }
            if (auto identity = ast::as<ast::Identity>(expr)) {
                return to_string(identity->expr);
            }
            return "";
        }
    };

    inline std::string type_to_string(const std::shared_ptr<ast::Type>& typ) {
        if (auto t = ast::as<ast::IntType>(typ)) {
            return std::string(t->is_signed ? "s" : "u") +
                   (t->endian == Endian::big      ? "b"
                    : t->endian == Endian::little ? "l"
                                                  : "") +
                   nums(*t->bit_size);
        }
        if (auto a = ast::as<ast::ArrayType>(typ)) {
            if (a->length_value) {
                return concat("[", nums(*a->length_value), "]", type_to_string(a->element_type), "");
            }
            return concat("[]", type_to_string(a->element_type), "");
        }
        if (auto fn = ast::as<ast::FunctionType>(typ)) {
            std::string args;
            for (auto& arg : fn->parameters) {
                if (!args.empty()) {
                    args += ", ";
                }
                args += type_to_string(arg);
            }
            return concat("fn (", args, ") -> ", type_to_string(fn->return_type));
        }
        if (auto s = ast::as<ast::IdentType>(typ)) {
            return s->ident->ident;
        }
        if (auto enum_type = ast::as<ast::EnumType>(typ)) {
            auto enum_ = enum_type->base.lock();
            if (enum_->base_type) {
                return enum_->ident->ident + "(" + type_to_string(enum_->base_type) + ")";
            }
            return enum_->ident->ident;
        }
        if (auto struct_type = ast::as<ast::StructType>(typ)) {
            if (auto member = ast::as<ast::Member>(struct_type->base.lock())) {
                if (auto fmt = ast::as<ast::Format>(member)) {
                    std::string cast_to;
                    for (auto& c : fmt->cast_fns) {
                        if (auto fn = c.lock()) {
                            if (!cast_to.empty()) {
                                cast_to += ",";
                            }
                            cast_to += type_to_string(fn->return_type);
                        }
                    }
                    if (cast_to.size()) {
                        return fmt->ident->ident + " cast(" + cast_to + ")";
                    }
                }
                return member->ident->ident;
            }
            return "(anonymous struct at " + nums(struct_type->loc.line) + ":" + nums(struct_type->loc.col) + ")";
        }
        if (auto struct_union_type = ast::as<ast::StructUnionType>(typ)) {
            return "(anonymous union of structs at " + nums(struct_union_type->loc.line) + ":" + nums(struct_union_type->loc.col) + ")";
        }
        if (auto union_type = ast::as<ast::UnionType>(typ)) {
            auto s = "(anonymous union at " + nums(union_type->loc.line) + ":" + nums(union_type->loc.col);
            if (union_type->common_type) {
                s += " with common type " + type_to_string(union_type->common_type);
            }
            s += ")";
            return s;
        }
        if (auto range = ast::as<ast::RangeType>(typ)) {
            if (!range->base_type) {
                return "range<any>";
            }
            return concat("range_", range->range.lock()->op == ast::BinaryOp::range_inclusive ? "inclusive" : "exclusive",
                          "<", type_to_string(range->base_type), ">");
        }
        return "(unknown type)";
    }
}  // namespace brgen::ast::tool
