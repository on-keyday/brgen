/*license*/
#pragma once
#include "nodes.h"

#include <string>
#include <vector>

namespace brgen::nast {

    struct PrintItem {
        std::string label;
        std::uint32_t id = 0;
        NodeType type{};
        bool weak = false;
        bool is_node = false;
        std::string scalar;
    };

    struct PrettyPrinter {
        Arena* arena = nullptr;
        std::string out;
        bool show_weak = true;
        bool show_null = false;

        explicit PrettyPrinter(Arena& a)
            : arena(&a) {}

        template <class T>
        void print(Node<T> root) {
            walk(root.id(), "", "", true);
        }

       private:
        static std::string scalar_of(const std::string& v) {
            return "\"" + v + "\"";
        }

        static std::string scalar_of(bool v) {
            return v ? "true" : "false";
        }

        static std::string scalar_of(const lexer::Loc& v) {
            return std::to_string(v.line) + ":" + std::to_string(v.col);
        }

        template <class V>
        static std::string scalar_of(const V& v) {
            if constexpr (std::is_enum_v<V>) {
                auto* s = to_string(v);
                return s ? std::string(s) : std::string("?");
            }
            else if constexpr (std::is_integral_v<V>) {
                return std::to_string(v);
            }
            else {
                return "...";
            }
        }

        template <class U>
        void add(std::vector<PrintItem>& items, const char* name, const Node<U>& v, bool weak) {
            if (weak && !show_weak) {
                return;
            }
            if (!show_null && v.is_null()) {
                return;
            }
            items.push_back(PrintItem{name, v.id(), v.type(), weak, true, {}});
        }

        template <class U>
        void add(std::vector<PrintItem>& items, const char* name, const std::vector<Node<U>>& v, bool weak) {
            if (weak && !show_weak) {
                return;
            }
            for (std::size_t i = 0; i < v.size(); i++) {
                items.push_back(PrintItem{std::string(name) + "[" + std::to_string(i) + "]",
                                          v[i].id(),
                                          v[i].type(),
                                          weak,
                                          true,
                                          {}});
            }
        }

        template <class V>
        void add(std::vector<PrintItem>& items, const char* name, const V& v, bool) {
            items.push_back(PrintItem{name, 0, NodeType{}, false, false, scalar_of(v)});
        }

        void walk(std::uint32_t id, const std::string& prefix, const std::string& branch, bool last) {
            out += prefix;
            out += branch;
            auto* h = arena->header_at(id);
            if (!h) {
                out += "(null)\n";
                return;
            }
            out += to_string(h->type);
            out += " #";
            out += std::to_string(id);
            if (h->loc.line) {
                out += " @";
                out += std::to_string(h->loc.line);
                out += ":";
                out += std::to_string(h->loc.col);
            }
            out += "\n";

            std::vector<PrintItem> items;
            auto index = h->data_index;
            visit_node_type(h->type, [&](auto tag) {
                using T = typename decltype(tag)::type;
                if (auto* d = arena->template data_at<T>(index)) {
                    d->for_each_field([&](const char* name, const auto& v, bool weak) {
                        add(items, name, v, weak);
                    });
                }
            });

            std::string child_prefix = prefix;
            if (!branch.empty()) {
                child_prefix += last ? "    " : "|   ";
            }
            for (std::size_t i = 0; i < items.size(); i++) {
                auto& it = items[i];
                bool is_last = i + 1 == items.size();
                std::string b = is_last ? "`-- " : "|-- ";
                if (!it.is_node) {
                    out += child_prefix + b + it.label + " = " + it.scalar + "\n";
                    continue;
                }
                // weak は所有しない逆向き参照。降りると循環するので参照だけ出す。
                if (it.weak) {
                    out += child_prefix + b + it.label + " -> ";
                    out += it.id ? (std::string(to_string(it.type)) + " #" + std::to_string(it.id))
                                 : std::string("(null)");
                    out += "\n";
                    continue;
                }
                walk(it.id, child_prefix, b + it.label + ": ", is_last);
            }
        }
    };

    template <class T>
    std::string pretty_print(Arena& a, Node<T> root) {
        PrettyPrinter p{a};
        p.print(root);
        return std::move(p.out);
    }

}  // namespace brgen::nast
