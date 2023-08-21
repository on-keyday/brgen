/*license*/
#pragma once
#include "writer.h"
#include <vector>
#include <optional>
#include <variant>
#include "core/common/util.h"
#include "core/common/error.h"

namespace brgen {
    namespace writer {
        using Writer = utils::code::CodeWriter<std::string, std::string_view>;

        struct Section : std::enable_shared_from_this<Section> {
           private:
            std::weak_ptr<Section> parent;
            std::optional<std::string> key;
            bool indent = false;
            Writer header;
            std::vector<std::variant<std::string, std::shared_ptr<Section>>> body;
            Writer footer;

           public:
            Writer& head() {
                return header;
            }

            Writer& foot() {
                return footer;
            }

            std::shared_ptr<Section> lookup(std::string_view path) {
                if (path.starts_with("/")) {
                    if (auto p = parent.lock()) {
                        return p->lookup(path);
                    }
                    path = path.substr(1);
                }
                auto pos = path.find("/");
                auto k = path.substr(0, pos);
                if (k == "." || k == "") {
                    if (pos == std::string_view::npos) {
                        return shared_from_this();
                    }
                    return lookup(path.substr(pos + 1));
                }
                if (k == "..") {
                    auto p = parent.lock();
                    if (!p) {
                        return p;
                    }
                    if (pos == std::string_view::npos) {
                        return p;
                    }
                    return p->lookup(path.substr(pos + 1));
                }
                for (auto& b : body) {
                    if (std::holds_alternative<std::shared_ptr<Section>>(b)) {
                        auto& f = std::get<std::shared_ptr<Section>>(b);
                        if (f->key && *f->key == k) {
                            if (pos == std::string_view::npos) {
                                return f;
                            }
                            return f->lookup(path.substr(pos + 1));
                        }
                    }
                }
                return nullptr;
            }

            expected<std::shared_ptr<Section>, std::string> add_section(std::string_view path, bool indent = false) {
                auto pos = path.find_last_of("/");
                std::shared_ptr<Section> section;
                auto target = path;
                if (pos != std::string_view::npos) {
                    auto contains = path.substr(0, pos);
                    section = lookup(contains);
                    if (!section) {
                        return unexpect(concat("lookup of ", contains, " failed"));
                    }
                    target = path.substr(pos + 1);
                }
                else {
                    section = shared_from_this();
                }
                if (target == "") {  // no name not allowed
                    return unexpect("empty path in unexpected");
                }
                auto set_new_section = [&] {
                    auto new_section = std::make_shared<Section>();
                    section->body.push_back(new_section);
                    new_section->parent = section;
                    new_section->indent = indent;
                    if (target != "." && target != "..") {
                        new_section->key = target;
                    }
                    return new_section;
                };
                if (target == ".") {  // anonymous
                    return set_new_section();
                }
                if (target == "..") {  // anonymous to parent
                    section = section->parent.lock();
                    if (!section) {
                        return unexpect(concat(path, " has no parent"));
                    }
                    return set_new_section();
                }
                for (auto& b : body) {
                    if (std::holds_alternative<std::shared_ptr<Section>>(b)) {
                        auto& f = std::get<std::shared_ptr<Section>>(b);
                        if (f->key && *f->key == target) {
                            return unexpect(concat(path, " already exists"));
                        }
                    }
                }
                return set_new_section();
            }

            void flush(auto& w) {
                w.write_unformatted(header.out());
                if (indent) {
                    auto s = w.indent_scope();
                    for (auto& b : body) {
                        if (std::holds_alternative<std::shared_ptr<Section>>(b)) {
                            auto& f = std::get<std::shared_ptr<Section>>(b);
                            f->flush(w);
                        }
                        else {
                            w.write_unformatted(std::get<std::string>(b));
                        }
                    }
                }
                else {
                    for (auto& b : body) {
                        if (std::holds_alternative<std::shared_ptr<Section>>(b)) {
                            auto& f = std::get<std::shared_ptr<Section>>(b);
                            f->flush(w);
                        }
                        else {
                            w.write_unformatted(std::get<std::string>(b));
                        }
                    }
                }
                w.write_unformatted(footer.out());
            }

            std::string flush() {
                std::string buf;
                utils::code::CodeWriter<std::string&, std::string_view> w{buf};
                flush(w);
                return std::move(buf);
            }

            void write(auto&&... a) {
                std::string buf;
                utils::strutil::appends(buf, a...);
                body.push_back(std::move(buf));
            }

            void writeln(auto&&... a) {
                std::string buf;
                utils::strutil::appends(buf, a..., "\n");
                body.push_back(std::move(buf));
            }

           private:
            void dump_section_impl(auto&& visit, std::string_view parent_path) {
                bool has_parent = !parent.expired();
                auto cur = has_parent ? concat(parent_path, "/", key ? *key : "(anonymous)") : "/";
                visit(cur);
                for (auto& b : body) {
                    if (auto g = std::get_if<std::shared_ptr<Section>>(&b)) {
                        (*g)->dump_section_impl(visit, has_parent ? cur : "");
                    }
                }
            }

           public:
            void dump_section(auto&& visit) {
                dump_section_impl(visit, "");
            }
        };

        inline std::shared_ptr<Section> root() {
            return std::make_shared<Section>();
        }

        using SectionPtr = std::shared_ptr<writer::Section>;
    }  // namespace writer
}  // namespace brgen
