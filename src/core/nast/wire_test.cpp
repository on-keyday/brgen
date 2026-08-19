/*license*/
// .bgn を nast のパーサに食わせ、アリーナと side table を線上表現へ移して
// encode -> decode -> 戻す、を往復させて元と突き合わせる。
//
//   nast_wire_test <file.bgn>...
//   nast_wire_test --error-tolerant <file.bgn>...
//
// 突き合わせは id ごとの直接比較。to_wire は id を線上に持たず並びで表すので、
// 往復して id がずれれば全フィールドの参照先がずれて必ず落ちる。
//
// nast_wire.hpp は wiregen.py -> src2json -> json2cpp2 で作った生成物。
// 作り直す手順は wiregen.py の docstring を見ること。
#include "nast_wire_conv.hpp"

#include "bind/binder.hpp"
#include "bind/import_resolver.hpp"
#include "bind/scope_resolver.hpp"
#include "parse.h"
#include "traverse.h"

#include <binary/reader.h>
#include <format>
#include <binary/writer.h>
#include <core/common/error.h>
#include <core/common/file.h>
#include <print>
#include <string>
#include <vector>

namespace {

    using namespace brgen::nast;

    struct Mismatch {
        std::uint32_t id = 0;
        std::string what;
    };

    // 2 つのアリーナの同じ id を突き合わせる。compare.h は 1 つのアリーナの中で
    // 2 つのノードを比べるものなので、こちらは別に書く。
    struct Comparer {
        Arena& lhs;
        Arena& rhs;
        std::vector<Mismatch> diffs;

        void note(std::uint32_t id, std::string what) {
            if (diffs.size() < 20) {
                diffs.push_back(Mismatch{id, std::move(what)});
            }
        }

        template <class V>
        bool same(const V& l, const V& r) {
            if constexpr (node_of<V>::is_node) {
                return l.id() == r.id() && l.type() == r.type();
            }
            else if constexpr (vector_of<V>::is_vector) {
                if (l.size() != r.size()) {
                    return false;
                }
                for (std::size_t i = 0; i < l.size(); i++) {
                    if (l[i].id() != r[i].id() || l[i].type() != r[i].type()) {
                        return false;
                    }
                }
                return true;
            }
            else {
                return l == r;
            }
        }

        void run() {
            if (lhs.node_count() != rhs.node_count()) {
                note(0, std::format("node count {} vs {}", lhs.node_count(), rhs.node_count()));
                return;
            }
            for (std::uint32_t id = 1; id <= lhs.node_count(); id++) {
                auto* lh = lhs.header_at(id);
                auto* rh = rhs.header_at(id);
                if (!lh || !rh) {
                    note(id, "missing header");
                    continue;
                }
                if (lh->type != rh->type) {
                    note(id, std::format("type {} vs {}", to_string(lh->type), to_string(rh->type)));
                    continue;
                }
                if (!(lh->loc == rh->loc)) {
                    note(id, "loc");
                }
                visit_node_type(lh->type, [&](auto tag) {
                    using T = typename decltype(tag)::type;
                    auto* ld = lhs.template data_at<T>(lh->data_index);
                    auto* rd = rhs.template data_at<T>(rh->data_index);
                    if (!ld || !rd) {
                        note(id, "missing data");
                        return;
                    }
                    ld->for_each_field(*rd, [&](const char* name, const auto& l, const auto& r,
                                                bool, bool) {
                        if (!same(l, r)) {
                            note(id, std::format("{}.{}", to_string(lh->type), name));
                        }
                    });
                });
            }
        }
    };

    // 表は数だけ見る。中身は表を作り直す経路が同じなので、数が合えば十分。
    std::string table_sizes(const SideTables& t) {
        std::string out;
        t.for_each_table([&](const char* name, const auto& table) {
            out += std::format("{}={} ", name, table.size());
        });
        return out;
    }

    struct Loaded {
        bool ok = false;
        std::string message;
    };

    Loaded load(const std::string& path, Arena& arena, Node<Module>& root, SideTables& tables,
                ParseOption popt) {
        // import した先も同じアリーナに入るので、往復の対象に含まれる。
        brgen::FileSet files;
        auto loaded = files.add_file(path);
        if (!loaded) {
            return {false, "cannot open file"};
        }
        auto* file = files.get_input(*loaded);
        if (!file) {
            return {false, "cannot read file"};
        }
        brgen::LocationError err;
        Context ctx;
        auto parsed = ctx.enter_stream(file, [&](Stream& s) {
            return parse(arena, s, &err, popt);
        });
        if (!parsed) {
            return {false, "parse error"};
        }
        root = *parsed;
        bind::ImportResolver importer{arena, tables, files, err, popt};
        importer.resolve(root);
        bind::ScopeResolver resolver{arena, tables, err};
        for (auto& mod : importer.modules) {
            bind::Binder binder{arena, err, tables};
            binder.bind(mod);
            resolver.resolve(mod);
        }
        return {true, {}};
    }

}  // namespace

int main(int argc, char** argv) {
    ParseOption popt;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--error-tolerant") {
            popt.error_tolerant = true;
        }
        else {
            paths.push_back(std::move(a));
        }
    }
    if (paths.empty()) {
        std::println(stderr, "usage: nast_wire_test [--error-tolerant] <file.bgn>...");
        return 2;
    }

    std::size_t ok = 0, ng = 0, skipped = 0;
    for (auto& path : paths) {
        Arena arena;
        SideTables tables;
        Node<Module> root;
        auto l = load(path, arena, root, tables, popt);
        if (!l.ok) {
            skipped++;
            std::println("skip  {:<52} {}", path, l.message);
            continue;
        }

        // pool は m を encode し終えるまで生かす。中の文字列は非所有ビュー。
        wire_conv::StringPool pool;
        wire::NastModule m;
        if (auto e = wire_conv::to_wire(arena, tables, root, pool, m)) {
            ng++;
            std::println("NG    {:<52} {}", path, e.error<std::string>());
            continue;
        }

        // 変換器も符号化器 (--use-error) も futils::error::Error<> を返す。
        // "to_wire: Format::name: set failed (node #10)" のように場所が出る。
        std::string buf;
        ::futils::binary::writer w{::futils::binary::resizable_buffer_writer<std::string>(), &buf};
        if (auto e = m.encode(w)) {
            ng++;
            std::println("NG    {:<52} encode: {}", path, e.error<std::string>());
            continue;
        }

        wire::NastModule got;
        ::futils::binary::reader r{::futils::view::rvec(buf.data(), buf.size())};
        if (auto e = got.decode(r)) {
            ng++;
            std::println("NG    {:<52} decode: {}", path, e.error<std::string>());
            continue;
        }

        Arena back;
        SideTables back_tables;
        Node<Module> back_root;
        if (auto e = wire_conv::from_wire(got, back, back_tables, back_root)) {
            ng++;
            std::println("NG    {:<52} {}", path, e.error<std::string>());
            continue;
        }

        Comparer cmp{arena, back, {}};
        cmp.run();
        bool root_ok = root.id() == back_root.id() && root.type() == back_root.type();
        auto lt = table_sizes(tables);
        auto rt = table_sizes(back_tables);
        if (!cmp.diffs.empty() || !root_ok || lt != rt) {
            ng++;
            std::println("NG    {:<52} {} nodes, {} bytes", path, arena.node_count(), buf.size());
            if (!root_ok) {
                std::println("        root #{} -> #{}", root.id(), back_root.id());
            }
            if (lt != rt) {
                std::println("        tables {}", lt);
                std::println("            -> {}", rt);
            }
            for (auto& d : cmp.diffs) {
                std::println("        #{} {}", d.id, d.what);
            }
        }
        else {
            ok++;
            std::println("ok    {:<52} {:>5} nodes  {:>7} bytes  {}", path, arena.node_count(),
                         buf.size(), lt);
        }
    }
    std::println("\n{} ok / {} mismatch / {} skipped", ok, ng, skipped);
    return ng == 0 ? 0 : 1;
}
