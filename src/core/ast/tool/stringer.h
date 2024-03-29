/*license*/
#pragma once
#include <string>
#include "../ast.h"
#include "../traverse.h"
#include <functional>
#include <unordered_map>
#include <env/env.h>

namespace brgen::ast::tool {

    void extract_ident(const std::shared_ptr<Expr>& expr, auto&& add_ident) {
        if (auto ident = ast::as<ast::Ident>(expr)) {
            add_ident(ast::cast_to<ast::Ident>(expr));
            return;
        }
        auto lookup = [&](auto&& f, const std::shared_ptr<Node>& v) -> void {
            if (ast::as<ast::Type>(v)) {
                return;
            }
            if (ast::as<ast::Ident>(v)) {
                add_ident(ast::cast_to<ast::Ident>(v));
                return;
            }
            traverse(v, [&](auto&& v) {
                f(f, v);
            });
        };
        traverse(expr, [&](auto&& v) -> void {
            lookup(lookup, v);
        });
    }

    std::optional<std::pair<std::shared_ptr<ast::Ident>, bool>> lookup_base(const std::shared_ptr<ast::Ident>& i) {
        if (!i) {
            return std::nullopt;
        }
        auto ref = i;
        bool via_member_access = false;
        for (;;) {
            auto base = ref->base.lock();
            if (!base) {
                return std::nullopt;
            }
            if (base->node_type == NodeType::member_access) {
                auto access = ast::cast_to<ast::MemberAccess>(base);
                auto target = access->base.lock();
                if (!target) {
                    return std::nullopt;
                }
                ref = std::move(target);
                via_member_access = true;
            }
            else if (base->node_type == NodeType::ident) {
                ref = ast::cast_to<ast::Ident>(base);
            }
            else {
                return std::pair{ref, via_member_access};
            }
        }
    }

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

        std::vector<std::shared_ptr<ast::Binary>> collect_defined_ident(const std::shared_ptr<ast::Expr>& expr) {
            std::vector<std::shared_ptr<ast::Binary>> defs;
            auto f = [&](auto&& f, auto&& v) -> void {
                ast::traverse(v, [&](auto&& v) -> void {
                    f(f, v);
                });
                if (auto ident = ast::as<ast::Ident>(v)) {
                    auto base = ast::as<ast::Ident>(ident->base.lock());
                    if (!base) {
                        return;
                    }
                    auto base_base = base->base.lock();
                    if (auto bin = ast::as<ast::Binary>(base_base)) {
                        if (bin->op == BinaryOp::define_assign ||
                            bin->op == BinaryOp::const_assign) {
                            defs.push_back(ast::cast_to<ast::Binary>(base_base));
                        }
                    }
                }
            };
            ast::traverse(expr, [&](auto&& v) -> void {
                f(f, v);
            });
            return defs;
        }
    };
}  // namespace brgen::ast::tool
