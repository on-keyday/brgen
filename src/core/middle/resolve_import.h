/*license*/
#pragma once
#include "../ast/ast.h"
#include "../ast/traverse.h"
#include "../common/file.h"
#include "../ast/parse.h"
#include "../ast/tool/extract_config.h"
#include "replacer.h"

namespace brgen::middle {
    struct PathStack {
        std::vector<fs::path> path_stack;
        std::map<fs::path, std::shared_ptr<ast::Program>> programs;

        fs::path current() {
            return path_stack.back();
        }

        void push(fs::path path) {
            path_stack.push_back(std::move(path));
        }

        void pop() {
            path_stack.pop_back();
        }

        std::shared_ptr<ast::Program> get(fs::path path) {
            for (auto& p : programs) {
                std::error_code ec;
                if (fs::equivalent(p.first, path, ec)) {
                    return p.second;
                }
                else if (ec) {
                    error({}, "cannot compare path ", p.first.generic_u8string(), " and ", path.generic_u8string(), " code=", ec.category().name(), ":", nums(ec.value())).report();
                }
            }
            return nullptr;
        }

        bool detect_circler(fs::path path, lexer::Loc loc) {
            for (auto& p : path_stack) {
                std::error_code ec;
                if (fs::equivalent(p, path, ec)) {
                    return true;
                }
                else if (ec) {
                    error(loc, "cannot compare path ", p.generic_u8string(), " and ", path.generic_u8string(), " code=", ec.category().name(), ":", nums(ec.value())).report();
                }
            }
            return false;
        }
    };

    inline result<void> resolve_import(std::shared_ptr<ast::Program>& n, FileSet& fs) {
        PathStack stack;
        auto l = fs.get_input(n->loc.file);
        if (!l) {
            return unexpect(error(n->loc, "cannot open file at index ", nums(n->loc.file)));
        }
        auto root_path = l->path();
        stack.push(root_path);
        stack.programs[root_path] = n;
        auto f = [&](auto&& f, NodeReplacer n) -> void {
            auto node = n.to_node();
            ast::traverse(node, [&](auto& g) {
                f(f, g);
            });
            std::optional<brgen::ast::tool::ConfigDesc> conf = ast::tool::extract_config(node, ast::tool::ExtractMode::call);
            if (conf) {
                if (conf->name != "config.import") {
                    return;
                }
                if (conf->arguments.size() != 1) {
                    error(conf->loc, "config.import() should take 1 argument but found ", nums(conf->arguments.size())).report();
                }
                if (conf->arguments[0]->node_type != ast::NodeType::str_literal) {
                    error(conf->loc, "config.import() should take string literal but found ", ast::node_type_to_string(conf->arguments[0]->node_type)).report();
                }
                ast::as<ast::MemberAccess>(ast::as<ast::Call>(node)->callee)->member->usage = ast::IdentUsage::reference_builtin_fn;
                auto raw_path = ast::cast_to<ast::StrLiteral>(conf->arguments[0]);
                auto path = unescape(raw_path->value);
                if (!path) {
                    error(conf->loc, "invalid path: cannot unescape ", raw_path->value).report();
                }
                auto res = fs.add_file(*path, true, stack.current().parent_path());
                if (!res) {
                    auto fullpath = stack.current().parent_path() / *path;
                    error(conf->loc, "cannot open file  ", fullpath.generic_u8string(), " code=", res.error().category().name(), ":", nums(res.error().value())).report();
                }
                auto new_input = fs.get_input(*res);
                if (!new_input) {
                    auto fullpath = fs.get_path(*res);
                    error(conf->loc, "cannot open file  ", fullpath.generic_u8string()).report();
                }
                auto new_path = new_input->path();
                if (stack.detect_circler(new_path, conf->loc)) {
                    error(conf->loc, "circular import detected: ", new_path.generic_u8string()).report();
                }
                auto found = stack.get(new_path);
                if (found) {
                    auto u8 = new_path.generic_u8string();
                    auto as_str = std::string(reinterpret_cast<const char*>(u8.c_str()), u8.size());
                    n.replace(std::make_shared<ast::Import>(ast::cast_to<ast::Call>(n.to_node()), std::move(found), std::move(as_str)));
                }
                else {
                    ast::Context c;
                    auto p = c.enter_stream(new_input, [&](ast::Stream& s) {
                        return ast::Parser{s}.parse();
                    });
                    if (!p) {
                        auto err = error(conf->loc, "cannot parse file ", new_path.generic_u8string());
                        for (LocationEntry& ent : p.error().locations) {
                            err.locations.push_back(std::move(ent));
                        }
                        err.report();
                    }
                    stack.push(new_path);
                    stack.programs[new_path] = *p;
                    f(f, *p);
                    stack.pop();
                    auto u8 = new_path.generic_u8string();
                    auto as_str = std::string(reinterpret_cast<const char*>(u8.c_str()), u8.size());
                    std::shared_ptr<ast::Import> imported = std::make_shared<ast::Import>(ast::cast_to<ast::Call>(std::move(node)), std::move(*p), std::move(as_str));
                    imported->import_desc->struct_type->base = imported;
                    n.replace(std::move(imported));
                }
            }
        };
        try {
            f(f, n);
        } catch (LocationError& e) {
            return unexpect(std::move(e));
        }
        return {};
    }
}  // namespace brgen::middle
