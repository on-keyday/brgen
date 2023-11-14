/*license*/
#pragma once
#include "../ast/ast.h"
#include "../ast/traverse.h"
#include "../common/file.h"
#include "../ast/parse.h"

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

        bool is_already_registered(fs::path path) {
            for (auto& p : path_stack) {
                std::error_code ec;
                if (fs::equivalent(p, path, ec)) {
                    return true;
                }
                else if (ec) {
                    error({}, "cannot compare path ", p.generic_u8string(), " and ", path.generic_u8string(), " code=", ec.category().name(), ":", nums(ec.value())).report();
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
        auto f = [&](auto&& f, auto& n) -> void {
            ast::traverse(n, [&](auto& g) {
                f(f, g);
            });
            using T = utils::helper::template_of_t<std::decay_t<decltype(n)>>;
            if constexpr (T::value) {
                using P = typename T::template param_at<0>;
                if constexpr (std::is_base_of_v<P, ast::Call> && !std::is_same_v<P, ast::Call>) {
                    if (ast::Call* m = ast::as<ast::Call>(n)) {
                        if (m->callee->node_type != ast::NodeType::member_access) {
                            return;
                        }
                        auto access = ast::cast_to<ast::MemberAccess>(m->callee);
                        if (access->target->node_type != ast::NodeType::config) {
                            return;
                        }
                        if (access->member->ident != "import") {
                            return;
                        }
                        if (m->arguments.size() != 1) {
                            error(m->loc, "config.import() should take 1 argument but found ", nums(m->arguments.size())).report();
                        }
                        if (m->arguments[0]->node_type != ast::NodeType::str_literal) {
                            error(m->loc, "config.import() should take string literal but found ", ast::node_type_to_string(m->arguments[0]->node_type)).report();
                        }
                        auto raw_path = ast::cast_to<ast::StrLiteral>(m->arguments[0]);
                        auto path = unescape(raw_path->value);
                        if (!path) {
                            error(m->loc, "invalid path: cannot unescape ", raw_path->value).report();
                        }
                        auto res = fs.add_file(*path, true, stack.current().parent_path());
                        if (!res) {
                            auto fullpath = stack.current().parent_path() / *path;
                            error(m->loc, "cannot open file  ", fullpath.generic_u8string(), " code=", res.error().category().name(), ":", nums(res.error().value())).report();
                        }
                        auto new_input = fs.get_input(*res);
                        if (!new_input) {
                            auto fullpath = fs.get_path(*res);
                            error(m->loc, "cannot open file  ", fullpath.generic_u8string()).report();
                        }
                        auto new_path = new_input->path();
                        if (stack.is_already_registered(new_path)) {
                            error(m->loc, "circular import detected: ", new_path.generic_u8string()).report();
                        }
                        auto found = stack.get(new_path);
                        if (found) {
                            auto u8 = new_path.generic_u8string();
                            auto as_str = std::string(reinterpret_cast<const char*>(u8.c_str()), u8.size());
                            n = std::make_shared<ast::Import>(ast::cast_to<ast::Call>(n), std::move(found), std::move(as_str));
                        }
                        else {
                            ast::Context c;
                            auto p = c.enter_stream(new_input, [&](ast::Stream& s) {
                                return ast::Parser{s}.parse();
                            });
                            if (!p) {
                                auto err = error(m->loc, "cannot parse file ", new_path.generic_u8string());
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
                            n = std::make_shared<ast::Import>(ast::cast_to<ast::Call>(n), std::move(*p), std::move(as_str));
                        }
                    }
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
