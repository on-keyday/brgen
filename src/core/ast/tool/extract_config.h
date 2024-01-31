/*license*/
#pragma once
#include "../ast.h"

namespace brgen::ast::tool {

    std::string extract_name(auto&& maybe_config, bool allow_ident = false) {
        if (!maybe_config) {
            return "";
        }
        if (auto c = ast::as<SpecialLiteral>(maybe_config)) {
            return to_string(c->kind);
        }
        if (auto d = ast::as<MemberAccess>(maybe_config)) {
            auto conf = extract_name(d->target);
            if (conf.size() == 0) {
                return "";
            }
            return conf + "." + d->member->ident;
        }
        if (allow_ident) {
            if (ast::Ident* c = ast::as<Ident>(maybe_config)) {
                return c->ident;
            }
        }
        return "";
    }

    struct ConfigDesc {
        std::string name;
        std::vector<std::shared_ptr<Expr>> arguments;
        bool assign_style = false;
        lexer::Loc loc;
    };

    enum class ExtractMode {
        both,
        assign,
        call,
    };

    std::optional<ConfigDesc> extract_config(auto&& node, ExtractMode mode = ExtractMode::both, bool allow_ident = false) {
        if (mode == ExtractMode::both || mode == ExtractMode::assign) {
            if (auto b = ast::as<Binary>(node); b && b->op == ast::BinaryOp::assign) {
                auto conf = extract_name(b->left, allow_ident);
                if (conf.size() == 0) {
                    return std::nullopt;
                }
                ConfigDesc desc;
                desc.name = conf;
                desc.assign_style = true;
                desc.arguments.push_back(b->right);
                desc.loc = b->loc;
                return desc;
            }
        }
        if (mode == ExtractMode::both || mode == ExtractMode::call) {
            if (auto c = ast::as<Call>(node)) {
                auto conf = extract_name(c->callee, allow_ident);
                if (conf.size() == 0) {
                    return std::nullopt;
                }
                ConfigDesc desc;
                desc.name = conf;
                desc.arguments = c->arguments;
                desc.loc = c->loc;
                return desc;
            }
        }
        return std::nullopt;
    }

}  // namespace brgen::ast::tool
