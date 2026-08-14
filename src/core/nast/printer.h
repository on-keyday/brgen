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

    struct PrintOptions {
        // weak を落とすと所有辺だけの純粋な木になる。
        bool show_weak = true;
        // null の Node フィールドは既定では項目ごと落とす。出すと
        // 「そのノードが持ちうるフィールド」が全部並ぶので、
        // parser がどこを埋め損ねたかを見るときに要る。
        bool show_null = false;
        // side table のエントリを、キーになっているノードの下に併記する。
        // binder が何をどこに書いたかを木の形のまま見るためのもの。
        bool show_tables = true;
    };

    struct PrettyPrinter {
        Arena* arena = nullptr;
        // 省略可。渡すと各ノードの下にそのノードを指す表の中身を出す。
        const SideTables* tables = nullptr;
        std::string out;
        bool show_weak = true;
        bool show_null = false;
        bool show_tables = true;

        explicit PrettyPrinter(Arena& a, PrintOptions opt = {})
            : arena(&a), show_weak(opt.show_weak), show_null(opt.show_null), show_tables(opt.show_tables) {}

        PrettyPrinter(Arena& a, const SideTables& t, PrintOptions opt = {})
            : arena(&a), tables(&t), show_weak(opt.show_weak), show_null(opt.show_null), show_tables(opt.show_tables) {}

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

        // このノードを指す side table の中身を項目として足す。
        // 表は node_type で型付けされているが contains/get は id しか見ないので、
        // 実行時の型で Node を組み直して引く。
        void add_table_entries(std::vector<PrintItem>& items, std::uint32_t id, NodeType type) {
            if (!tables || !show_tables) {
                return;
            }
            tables->for_each_table([&](const char* table_name, const auto& table) {
                using table_t = std::decay_t<decltype(table)>;
                using key_node = Node<typename table_t::node_type>;
                auto key = key_node::from_unique_id((std::uint64_t(type) << 32) | id);
                if (!table.contains(key)) {
                    return;
                }
                if constexpr (requires { table.get(key); }) {
                    if (const auto* entry = table.get(key)) {
                        entry->for_each_field([&](const char* field, const auto& v, bool) {
                            // 表の中の Node は所有辺ではない。降りると
                            // Ident -> [Resolution].target -> Field -> name -> Ident で回る。
                            add(items, (std::string("[") + table_name + "]." + field).c_str(), v, true);
                        });
                        return;
                    }
                }
                // flag は値を持たないので、在ることだけを出す
                items.push_back(PrintItem{std::string("[") + table_name + "]", 0, NodeType{},
                                          false, false, "true"});
            });
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
            add_table_entries(items, id, h->type);
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
                    if (it.id) {
                        out += to_string(it.type);
                        out += " #";
                        out += std::to_string(it.id);
                        // 参照先には降りないので、どこを指しているかは位置でしか分からない。
                        // 木に現れないノード (side table 経由の合成ノードなど) では特に要る。
                        if (auto* h = arena->header_at(it.id); h && h->loc.line) {
                            out += " @";
                            out += std::to_string(h->loc.line);
                            out += ":";
                            out += std::to_string(h->loc.col);
                        }
                    }
                    else {
                        out += "(null)";
                    }
                    out += "\n";
                    continue;
                }
                walk(it.id, child_prefix, b + it.label + ": ", is_last);
            }
        }
    };

    template <class T>
    std::string pretty_print(Arena& a, Node<T> root, PrintOptions opt = {}) {
        PrettyPrinter p{a, opt};
        p.print(root);
        return std::move(p.out);
    }

    // 表つき。binder が何をどこに書いたかを木の形のまま見る。
    template <class T>
    std::string pretty_print(Arena& a, const SideTables& tables, Node<T> root, PrintOptions opt = {}) {
        PrettyPrinter p{a, tables, opt};
        p.print(root);
        return std::move(p.out);
    }

}  // namespace brgen::nast
