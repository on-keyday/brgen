/*license*/
#include "session.hpp"
#include "../node/printer.h"
#include "../node/traverse.h"
#include "../node/util.h"
#include "../parse/unparse.h"

#include <format>
#include <istream>
#include <map>
#include <ostream>
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

    std::string Session::help() {
        return
            "  p <id> [depth]      ノードを木で。side table のエントリも一緒に出る (既定 depth 2)\n"
            "  pp <id>             深さ無制限の p\n"
            "  u <id>              .bgn に綴り戻す\n"
            "  src <id>            原文のその位置\n"
            "  up <id>             所有辺で 1 つ上\n"
            "  find <Kind> [f=v]   その種別のノード。f=v はノードでないフィールドの一致で絞る\n"
            "  kinds               出ている NodeType と件数\n"
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
            auto text = unparse_node(a, node_at(id));
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
                out += "Kind が要る (kinds で一覧)\n";
                return true;
            }
            auto type = from_string<NodeType>(kind);
            if (!type) {
                out += std::format("そんな NodeType は無い: {}\n", kind);
                return true;
            }
            std::string filter, key, value;
            in >> filter;
            if (!filter.empty()) {
                auto eq = filter.find('=');
                if (eq == std::string::npos) {
                    out += "絞り込みは field=value の形\n";
                    return true;
                }
                key = filter.substr(0, eq);
                value = filter.substr(eq + 1);
            }
            auto hit = find(*type, key, value);
            for (auto n : hit) {
                out += headline(n) + "\n";
            }
            out += std::format("{} 件\n", hit.size());
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

    void repl(const Session& s, std::istream& in, std::ostream& out) {
        std::string line;
        while (true) {
            out << "nast> " << std::flush;
            if (!std::getline(in, line)) {
                out << "\n";
                return;
            }
            std::string buf;
            auto go = s.run(line, buf);
            out << buf << std::flush;
            if (!go) {
                return;
            }
        }
    }

}  // namespace brgen::nast::query
