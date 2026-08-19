/*license*/
// wiregen.py が出した .bgn を src2json -> json2cpp2 に通して得た符号化器の
// 往復テスト。nast の状態を brgen 自身でバイナリにできることの確認用で、
// アリーナ本体とは独立している (nodes.h には依存しない)。
//
// nast_wire.bgn と nast_wire.hpp は生成物だが、EBM の
// extended_binary_module.bgn / .hpp と同じく追跡する。作り直す手順:
//
//   python src/core/nast/wiregen.py
//   ./tool/src2json src/core/nast/nast_wire.bgn > ignore/nast/nast_wire.json
//   ./tool/json2cpp2 -f ignore/nast/nast_wire.json > src/core/nast/nast_wire.hpp
//
// このテストの組み立て:
//
//   clang++ -std=c++23 -I src/core/nast -I <futils>/src/include \
//       src/core/nast/wire_test.cpp -o ignore/nast/wire_test
#include "nast_wire.hpp"
#include <binary/writer.h>
#include <binary/reader.h>
#include <print>
#include <string>
#include <vector>

using namespace brgen::nast::wire;

static Ref mkref(std::uint32_t id) { Ref r; r.id = id; return r; }

int main() {
    NastModule m;
    m.version = 1;

    // string table: 0 = "", 1 = "Hello"
    std::vector<std::string> strs{"", "Hello"};
    std::vector<StringEntry> entries;
    for (auto& s : strs) {
        StringEntry e;
        e.set_data(::futils::view::rvec(s.data(), s.size()));
        entries.push_back(e);
    }
    m.set_strings(entries);

    // node 0: Ident("Hello")
    Node ident;
    ident.node_kind = NodeKind::Ident;
    ident.loc.line = 3; ident.loc.col = 1;
    if (!ident.identifier(mkref(1))) { std::print("set identifier failed\n"); return 1; }

    // node 1: Format { name -> #0 }
    Node fmt;
    fmt.node_kind = NodeKind::Format;
    fmt.loc.line = 3; fmt.loc.col = 8;
    if (!fmt.name(mkref(0))) { std::print("set name failed\n"); return 1; }

    // node 2: Module { statements = [#1] }
    Node mod;
    mod.node_kind = NodeKind::Module;
    mod.loc.line = 1; mod.loc.col = 1;
    if (!mod.statements(std::vector<Ref>{mkref(1)})) { std::print("set statements failed\n"); return 1; }

    m.set_nodes(std::vector<Node>{ident, fmt, mod});
    m.root = mkref(2);

    std::string buf;
    ::futils::binary::writer w{::futils::binary::resizable_buffer_writer<std::string>(), &buf};
    if (!m.encode(w)) { std::print("encode failed\n"); return 1; }
    std::print("encoded {} bytes\n", buf.size());

    NastModule got;
    ::futils::binary::reader r{::futils::view::rvec(buf.data(), buf.size())};
    if (!got.decode(r)) { std::print("decode failed\n"); return 1; }

    bool ok = true;
    auto check = [&](const char* what, bool cond) {
        if (!cond) { std::print("MISMATCH: {}\n", what); ok = false; }
    };
    check("version", got.version == 1);
    check("strings_len", got.strings.size() == 2);
    check("string[1]", got.strings.size() > 1 &&
          std::string((const char*)got.strings[1].data.data(), got.strings[1].data.size()) == "Hello");
    check("nodes_len", got.nodes.size() == 3);
    check("root", got.root.id == 2);
    if (got.nodes.size() == 3) {
        check("node0 kind", got.nodes[0].node_kind == NodeKind::Ident);
        auto* idr = got.nodes[0].identifier();
        check("node0 identifier", idr && idr->id == 1);
        check("node0 loc", got.nodes[0].loc.line == 3 && got.nodes[0].loc.col == 1);
        check("node1 kind", got.nodes[1].node_kind == NodeKind::Format);
        auto* nm = got.nodes[1].name();
        check("node1 name", nm && nm->id == 0);
        // Format has no `identifier` field -> accessor must report absence
        check("node1 has no identifier", got.nodes[1].identifier() == nullptr);
        check("node2 kind", got.nodes[2].node_kind == NodeKind::Module);
        auto* st = got.nodes[2].statements();
        check("node2 statements", st && st->size() == 1 && (*st)[0].id == 1);
    }
    std::print("{}\n", ok ? "ROUNDTRIP OK" : "ROUNDTRIP FAILED");
    return ok ? 0 : 1;
}
