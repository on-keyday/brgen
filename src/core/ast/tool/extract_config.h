/*license*/
#pragma once
#include "../ast.h"

namespace brgen::ast::tool {

    std::string extract_config_name(auto&& maybe_config) {
        if (!maybe_config) {
            return "";
        }
        if (auto c = ast::as<Config>(maybe_config)) {
            return "config";
        }
        if (auto c = ast::as<Input>(maybe_config)) {
            return "input";
        }
        if (auto c = ast::as<Output>(maybe_config)) {
            return "output";
        }
        if (auto d = ast::as<MemberAccess>(maybe_config)) {
            auto conf = extract_config_name(d->target);
            if (conf.size() == 0) {
                return "";
            }
            return conf + "." + d->member->ident;
        }
        return "";
    }

    struct ConfigDesc {
        std::string name;
        std::vector<std::shared_ptr<Expr>> arguments;
        bool assign_style = false;
        lexer::Loc loc;
    };

    std::optional<ConfigDesc> extract_config(auto&& node) {
        if (auto b = ast::as<Binary>(node); b && b->op == ast::BinaryOp::assign) {
            auto conf = extract_config_name(b->left);
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
        if (auto c = ast::as<Call>(node)) {
            auto conf = extract_config_name(c->callee);
            if (conf.size() == 0) {
                return std::nullopt;
            }
            ConfigDesc desc;
            desc.name = conf;
            desc.arguments = c->arguments;
            desc.loc = c->loc;
            return desc;
        }
        return std::nullopt;
    }
}  // namespace brgen::ast::tool
