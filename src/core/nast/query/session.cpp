/*license*/
#include "session.hpp"
#include "lower_view.hpp"
#include "../node/printer.h"
#include "../node/traverse.h"
#include "../node/util.h"
#include "../parse/unparse.h"

#include <format>
#include <iostream>
#include <map>
#include <sstream>

namespace brgen::nast::query {

    namespace {
        // 木から外れたノードは親を持たない。見出しでそう言うために使う。
        constexpr std::size_t headline_text_limit = 60;
    }  // namespace

    Session::Session(Program& prog) : p(prog) {
        auto index = [&](auto&& self, NodeAny id) -> void {
            if (!id || !reachable.insert(id.id()).second) {
                return;
            }
            traverse(p.arena, id, [&](NodeAny child) {
                parent.emplace(child.id(), id.id());
                self(self, child);
            });
        };
        for (auto& mod : p.modules) {
            index(index, mod);
        }
    }

    NodeAny Session::node_at(std::uint32_t id) const {
        auto* h = p.arena.header_at(id);
        if (!h) {
            return nullref;
        }
        return NodeAny::from_unique_id((std::uint64_t(h->type) << 32) | id);
    }

    std::string Session::headline(std::uint32_t id) const {
        auto* h = p.arena.header_at(id);
        if (!h) {
            return std::format("#{} (no such node)", id);
        }
        auto& a = const_cast<Arena&>(p.arena);
        std::string out = std::format("#{:<6} {:<20}", id, to_string(h->type));
        if (h->loc.line) {
            out += std::format(" @{}:{}", h->loc.line, h->loc.col);
        }
        if (!reachable.contains(id)) {
            out += " (孤児)";
        }
        auto n = node_at(id);
        if (auto st = n.as_any<Statement>(); st && !name_of(a, st).empty()) {
            out += "  " + std::string(name_of(a, st));
        }
        else if (n.as_any<Expr>() || n.as_any<Statement>()) {
            auto text = unparse_node(a, n);
            if (auto nl = text.find('\n'); nl != std::string::npos) {
                text = text.substr(0, nl) + " ...";
            }
            if (text.size() > headline_text_limit) {
                text = text.substr(0, headline_text_limit) + " ...";
            }
            if (!text.empty()) {
                out += "  " + text;
            }
        }
        return out;
    }

    std::string Session::print_node(std::uint32_t id, std::size_t depth) const {
        auto& a = const_cast<Arena&>(p.arena);
        PrintOptions opt;
        opt.max_depth = depth;
        PrettyPrinter pr{a, p.tables, opt};
        pr.print_id(id);
        return std::move(pr.out);
    }

    std::string Session::source_at(std::uint32_t id) const {
        auto* h = p.arena.header_at(id);
        if (!h || !h->loc.line) {
            return "(位置なし)\n";
        }
        // 診断と同じ体裁で出す。原文の行と桁の指し示しは FileSet が持っている。
        auto entry = const_cast<FileSet&>(p.files).error("", h->loc);
        std::string buf;
        entry.omit_error(buf);
        if (buf.empty() || buf.back() != '\n') {
            buf += '\n';
        }
        return buf;
    }

    std::optional<std::string> Session::field_text(std::uint32_t id, std::string_view want) const {
        auto* h = p.arena.header_at(id);
        if (!h) {
            return std::nullopt;
        }
        auto& a = const_cast<Arena&>(p.arena);
        std::optional<std::string> found;
        auto data_index = h->data_index;
        visit_node_type(h->type, [&](auto tag) {
            using T = typename decltype(tag)::type;
            auto* d = a.template data_at<T>(data_index);
            if (!d) {
                return;
            }
            d->for_each_field([&](const char* name, const auto& v, bool) {
                using M = std::decay_t<decltype(v)>;
                if constexpr (!node_of<M>::is_node && !vector_of<M>::is_vector) {
                    if (want == name) {
                        found = scalar_text(v);
                    }
                }
            });
        });
        return found;
    }

    std::vector<std::uint32_t> Session::find(NodeType kind, std::string_view key,
                                             std::string_view value) const {
        std::vector<std::uint32_t> out;
        for (std::uint32_t id = 1; id <= p.arena.node_count(); id++) {
            auto* h = p.arena.header_at(id);
            if (!h || h->type != kind) {
                continue;
            }
            if (!key.empty()) {
                auto got = field_text(id, key);
                if (!got) {
                    continue;
                }
                // 文字列は print_node と同じく引用符つきで出るので、素で
                // 書かれても当たるように両方見る。
                if (*got != value && *got != "\"" + std::string(value) + "\"") {
                    continue;
                }
            }
            out.push_back(id);
        }
        return out;
    }

    std::vector<std::uint32_t> Session::select(std::optional<NodeType> kind,
                                               const FilterPtr& filter) const {
        std::vector<std::uint32_t> out;
        for (std::uint32_t id = 1; id <= p.arena.node_count(); id++) {
            auto* h = p.arena.header_at(id);
            if (!h) {
                continue;
            }
            if (kind && h->type != *kind) {
                continue;
            }
            if (filter && !matches(*this, id, *filter)) {
                continue;
            }
            out.push_back(id);
        }
        return out;
    }

    namespace {
        template <class M>
        std::string field_type_name(const M&) {
            if constexpr (node_of<M>::is_node) {
                return std::string("Node<") + to_string(get_node_type<typename node_of<M>::type>()) + ">";
            }
            else if constexpr (vector_of<M>::is_vector) {
                return std::string("[") + to_string(get_node_type<typename vector_of<M>::type>()) + "]";
            }
            else if constexpr (std::is_same_v<M, std::string>) {
                return "string";
            }
            else if constexpr (std::is_same_v<M, bool>) {
                return "bool";
            }
            else if constexpr (std::is_enum_v<M>) {
                std::string s = "enum{";
                bool first = true;
                for (auto& [v, name] : enum_array<M>) {
                    if (!first) {
                        s += "|";
                    }
                    s += std::string(name);
                    first = false;
                }
                return s + "}";
            }
            else if constexpr (std::is_integral_v<M>) {
                return "int";
            }
            else {
                return "loc";
            }
        }
    }  // namespace

    std::string Session::show_kind(NodeType kind) const {
        std::string out = std::format("{} {{\n", to_string(kind));
        visit_node_type(kind, [&](auto tag) {
            using T = typename decltype(tag)::type;
            NodeData<T> d{};
            d.for_each_field([&](const char* name, const auto& v, bool weak) {
                out += std::format("    {:<18} {}{}\n", name, field_type_name(v), weak ? "  (weak)" : "");
            });
        });
        out += "}\n";
        std::string tables;
        p.tables.for_each_table([&](const char* name, const auto& table) {
            using table_t = std::decay_t<decltype(table)>;
            if (is_derived<typename table_t::node_type>(kind)) {
                tables += std::format("    @{}\n", name);
            }
        });
        if (!tables.empty()) {
            out += "side tables:\n" + tables;
        }
        return out;
    }

    std::string Session::list_kinds() {
        std::string out;
        std::size_t col = 0;
        for (auto& [v, name] : enum_array<NodeType>) {
            out += std::format("{:<26}", name);
            if (++col % 3 == 0) {
                out += "\n";
            }
        }
        if (col % 3) {
            out += "\n";
        }
        return out;
    }

    std::string Session::help() {
        return
            "  p <id> [depth]      ノードを木で。side table のエントリも一緒に出る (既定 depth 2)\n"
            "  pp <id>             深さ無制限の p\n"
            "  u <id> [self]       .bgn に綴り戻す。self を付けると暗黙のレシーバも綴る\n"
            "  src <id>            原文のその位置\n"
            "  up <id>             所有辺で 1 つ上\n"
            "  find <Kind> [{ 条件 }]  その種別のノード (Any で全部)。条件の綴りは query/filter.hpp\n"
            "      例: find Field { type.kind == IntType and type.bit_size == 8 }\n"
            "          find Ident { @Resolution.target.kind == Field }\n"
            "          find Any { line == 52 }\n"
            "  lower <id>          そのノードに当てはまる lowering 規則を当てて綴る\n"
            "  show [<Kind>]       その種のフィールド名と型 (引数なしで全 NodeType)\n"
            "  kinds               この木に出ている NodeType と件数\n"
            "  stat                ノード数と到達可能数\n"
            "  help / quit\n"
            "\n"
            "  値の綴りは p が出すものと同じ (文字列は \"...\"、列挙は名前)。\n";
    }

    bool Session::run(std::string_view line, std::string& out) const {
        std::istringstream in{std::string(line)};
        std::string cmd;
        if (!(in >> cmd)) {
            return true;
        }
        auto& a = const_cast<Arena&>(p.arena);
        std::uint32_t id = 0;
        auto want_id = [&] {
            if (!(in >> id)) {
                out += "id が要る\n";
                return false;
            }
            if (!a.header_at(id)) {
                out += std::format("#{} は無い\n", id);
                return false;
            }
            return true;
        };

        if (cmd == "quit" || cmd == "exit" || cmd == "q") {
            return false;
        }
        if (cmd == "help" || cmd == "h" || cmd == "?") {
            out += help();
            return true;
        }
        if (cmd == "p" || cmd == "pp") {
            if (!want_id()) {
                return true;
            }
            std::size_t depth = cmd == "pp" ? 0 : 2;
            if (std::size_t got = 0; cmd == "p" && (in >> got)) {
                depth = got;
            }
            out += print_node(id, depth);
            return true;
        }
        if (cmd == "u") {
            if (!want_id()) {
                return true;
            }
            // `u <id> self` で、bind/receiver が足した暗黙のレシーバも綴る。
            std::string how;
            in >> how;
            auto text = unparse_node(a, node_at(id), UnparseOption{.explicit_self = how == "self"});
            out += text.empty() ? "(綴れない)" : text;
            out += "\n";
            return true;
        }
        if (cmd == "src") {
            if (!want_id()) {
                return true;
            }
            out += source_at(id);
            return true;
        }
        if (cmd == "up") {
            if (!want_id()) {
                return true;
            }
            auto it = parent.find(id);
            if (it == parent.end()) {
                out += "(親なし。Module か、木から外れたノード)\n";
                return true;
            }
            out += headline(it->second) + "\n";
            return true;
        }
        if (cmd == "find") {
            std::string kind;
            if (!(in >> kind)) {
                out += "Kind が要る (kinds で一覧、Any で全部)\n";
                return true;
            }
            std::optional<NodeType> type;
            if (kind != "Any") {
                type = from_string<NodeType>(kind);
                if (!type) {
                    out += std::format("そんな NodeType は無い: {}\n", kind);
                    return true;
                }
            }
            // 残りは条件式。`{ ... }` でも裸でも受ける。`f=v` は昔の短縮形なので
            // `f == v` に読み替えて同じ経路に乗せる。
            std::string rest;
            std::getline(in, rest);
            auto trim = [](std::string& v) {
                while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) {
                    v.erase(v.begin());
                }
                while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) {
                    v.pop_back();
                }
            };
            trim(rest);
            if (rest.size() >= 2 && rest.front() == '{' && rest.back() == '}') {
                rest = rest.substr(1, rest.size() - 2);
                trim(rest);
            }
            else if (auto eq = rest.find('='); eq != std::string::npos &&
                                               rest.find("==") == std::string::npos &&
                                               rest.find('<') == std::string::npos &&
                                               rest.find('>') == std::string::npos &&
                                               rest.find('!') == std::string::npos) {
                rest = rest.substr(0, eq) + " == " + rest.substr(eq + 1);
            }
            FilterPtr filter;
            if (!rest.empty()) {
                std::string err;
                filter = parse_filter(rest, err);
                if (!filter) {
                    out += err + "\n";
                    return true;
                }
            }
            auto hit = select(type, filter);
            for (auto n : hit) {
                out += headline(n) + "\n";
            }
            out += std::format("{} 件\n", hit.size());
            return true;
        }
        if (cmd == "lower") {
            if (!want_id()) {
                return true;
            }
            auto n = node_at(id);
            auto text = lower_text(const_cast<Program&>(p), n);
            out += text.empty() ? "(この種に当てはまる規則は無い)\n" : text;
            // 結果のノードは表に載っているので id を出す。そのまま p / u で
            // 追える (field の読み書きだけは表に載らないので出ない)。
            auto note = [&](const char* label, NodeAny made) {
                if (made) {
                    out += std::format("[{} #{}]\n", label, made.id());
                }
            };
            if (auto av = n.as_any<Available>()) {
                if (auto* e = p.tables.table<LoweredAvailable>().get(av)) {
                    note("expr", e->expr);
                }
            }
            else if (auto bin = n.as_any<Binary>()) {
                if (auto* e = p.tables.table<LoweredRangeCompare>().get(bin)) {
                    note("expr", e->expr);
                }
            }
            else if (auto m = n.as_any<Match>()) {
                if (auto* e = p.tables.table<LoweredMatch>().get(m)) {
                    note("branch", e->branch);
                }
            }
            else if (auto cond = n.as_any<Cond>()) {
                if (auto* e = p.tables.table<LoweredCond>().get(cond)) {
                    note("branch", e->branch);
                    note("value", e->value);
                }
            }
            return true;
        }
        if (cmd == "show") {
            std::string kind;
            if (!(in >> kind)) {
                out += list_kinds();
                return true;
            }
            auto type = from_string<NodeType>(kind);
            if (!type) {
                out += std::format("そんな NodeType は無い: {}\n", kind);
                return true;
            }
            out += show_kind(*type);
            return true;
        }
        if (cmd == "kinds") {
            std::map<std::string, std::size_t> hist;
            for (std::uint32_t i = 1; i <= p.arena.node_count(); i++) {
                if (auto* h = p.arena.header_at(i)) {
                    hist[to_string(h->type)]++;
                }
            }
            for (auto& [k, v] : hist) {
                out += std::format("{:<24} {:>6}\n", k, v);
            }
            return true;
        }
        if (cmd == "stat") {
            auto total = p.arena.node_count();
            out += std::format("arena {} nodes, reachable {} ({:.1f}%)\n", total, reachable.size(),
                               total ? 100.0 * double(reachable.size()) / double(total) : 0.0);
            out += std::format("modules {}, root #{}\n", p.modules.size(), p.root.id());
            return true;
        }
        out += std::format("知らないコマンド: {} (help)\n", cmd);
        return true;
    }

    void repl(const Session& s, futils::wrap::UtfIn& in, futils::wrap::UtfOut& out) {
        std::string line;
        while (true) {
            out << "nast> ";
            line.clear();
            in >> line;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            if (line.empty() && in.eof()) {
                out << "\n";
                return;
            }
            std::string buf;
            auto go = s.run(line, buf);
            out << buf;
            if (!go) {
                return;
            }
        }
    }

}  // namespace brgen::nast::query
