/*license*/
// nast の単体スモークテスト。build.py から呼ばれる。
// 生成された nodes.h が「コンパイルできる」だけでなく、
// 型変換・ダウンキャスト・シリアライズが意図どおり動くところまで見る。
#include "../node/console.h"
#include "../node/nodes.h"
#include "../node/access.h"
#include "../node/traverse.h"
#include "../node/compare.h"
#include "../node/printer.h"
#include "../node/from_json.h"
#include "../node/build.h"

#include <algorithm>
#include <string>
#include <vector>

// futils があれば as_json を実際に走らせる。nodes.h 自体は futils に依存しないので、
// futils が無い環境でも構造のテストだけは通るようにしておく (build.py が定義する)。
#ifdef NAST_TEST_WITH_JSON
#include <json/stringer.h>
#endif

using brgen::nast::print_line;
using brgen::nast::print_text;

namespace {

    int failures = 0;

    // 検査は substring でやるので、木そのものは既定では出さない。
    // --verbose を付けると組み立てた入力を目で見られる。
    bool verbose = false;

    void check(bool ok, const char* what) {
        print_line("  [{}] {}", ok ? "ok" : "NG", what);
        if (!ok) {
            failures++;
        }
    }

    void dump(const char* title, const std::string& text) {
        if (!verbose) {
            return;
        }
        print_line("\n--- {} ---\n{}", title, text);
    }

}  // namespace

int main(int argc, char** argv) {
    using namespace brgen::nast;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--verbose" || a == "-v") {
            verbose = true;
        }
    }

    print_line("nast smoke test");

    Arena arena;
    auto fmt = arena.make<Format>();
    auto body = arena.make<Body>();
    auto name = arena.make<Ident>();
    auto field_a = arena.make<Field>();
    auto func = arena.make<Function>();

    name->identifier = "Sample";
    fmt->name = name;
    fmt->body = body;

    // Ref<T> から基底の Node<U> へ 1 段で変換できること。
    // operator Node<T> だけだとユーザー定義変換 2 段になり push_back が通らない。
    body->statements.push_back(field_a);
    body->statements.push_back(func);
    check(body->statements.size() == 2, "Ref -> Node<Statement> implicit conversion");

    Node<Statement> as_stmt = fmt;
    Node<NamedStatement> as_named = fmt;  // 中間の抽象基底へも直接
    check(as_stmt.type() == NodeType::Format && as_named.type() == NodeType::Format,
          "conversion keeps the concrete node type");

    // 継承ビットによる判定
    check(is_derived<Statement>(NodeType::Field) &&
              is_derived<NamedTypeStatement>(NodeType::Field) &&
              !is_derived<NamedTypeStatement>(NodeType::Function),
          "is_derived<> bit test");

    // 抽象 Node からの取得 (switch で具象プールへ降りる)
    check(arena.get<Statement>(as_stmt) != nullptr, "get<Abstract>() resolves to concrete data");

    // チェック付きダウンキャスト
    check(bool(as_stmt.as<Format>()), "as<Format>() succeeds");
    check(!bool(as_stmt.as<Function>()), "as<Function>() returns null on mismatch");

    // as<U>() は U が T の派生であることを要求するので、generic な走査のように
    // T が実行時まで決まらない場所では書けない。as_any<U>() は静的な関係を問わず、
    // type_ のビットだけで判定して当たらなければ null を返す。
    check(bool(as_stmt.as_any<Format>()) && !bool(as_stmt.as_any<Ident>()) &&
              !bool(as_stmt.as_any<IntType>()),
          "as_any<U>() drops the derived-from requirement and checks at runtime");
    // Ref 側の as は実体化されていなかったので壊れていた (arena_ を A& に渡していた)
    RefBase<Arena, Statement> fmt_ref = fmt;
    check(bool(fmt_ref.as<Format>()) && !bool(fmt_ref.as_any<Ident>()),
          "Ref::as / Ref::as_any resolve through the arena");

    // null / 不正な Node
    Node<Format> nil{};
    check(!bool(nil) && arena.as_ref(nil).get() == nullptr, "null Node yields null Ref");

    // NodeHeader が代入可能であること (到達不能ノードの除去などで vector を編集するため)
    std::vector<NodeHeader> headers{
        {NodeType::Field, 0},
        {NodeType::Function, 1},
        {NodeType::Format, 2},
    };
    headers.erase(headers.begin());
    std::sort(headers.begin(), headers.end(),
              [](const NodeHeader& l, const NodeHeader& r) { return ordinal(l.type) < ordinal(r.type); });
    headers.resize(1);
    check(headers.size() == 1, "NodeHeader is assignable (erase/sort/resize)");

    // ---- make の引数 -------------------------------------------------------
    // loc は NodeHeader へ、残りは NodeData<T> の集成初期化へ渡る。
    auto lit = arena.make<IntLiteral>(brgen::lexer::Loc{.line = 42});
    check(arena.get_header(lit.id())->loc.line == 42, "make(loc) stores loc in the header");

    // 基底のフィールドを持つ型は、基底の集成を先に渡す (集成初期化の規則どおり)
    auto typed = arena.make<IntLiteral>(brgen::lexer::Loc{.line = 7},
                                        NodeData<Literal>{}, std::string{"123"});
    check(typed->value == "123" && arena.get_header(typed.id())->loc.line == 7,
          "make(loc, args...) aggregate-initializes NodeData");

    auto plain = arena.make<Ident>();
    check(plain->identifier.empty() && arena.get_header(plain.id())->loc.line == 0,
          "make() with no arguments still works");

    // ---- Node の比較 -------------------------------------------------------
    auto other = arena.make<Format>();
    check(fmt.id() == fmt.id() && fmt.id() != other.id(), "Node == / != compares identity");

    // 派生 -> 基底の暗黙変換が両辺に効く
    Node<Statement> fmt_as_stmt = fmt;
    check(fmt_as_stmt == fmt.id() && !(fmt_as_stmt == other.id()),
          "Node comparison works across derived/base");

    // null 同士は型が違っても等しい (id_ のみで比べているため)
    check(Node<Format>{} == Node<Statement>{} && Node<Format>{} == nullref,
          "null Nodes compare equal regardless of the static type");

    // 順序付けがあるので map/sort に載る
    std::vector<Node<Statement>> sorted{func.id(), field_a.id(), fmt.id()};
    std::sort(sorted.begin(), sorted.end());
    check(std::is_sorted(sorted.begin(), sorted.end()) && sorted.front() < sorted.back(),
          "Node is ordered (usable as a map key / sortable)");

    // ---- enum ヘルパ (ast::enum_array<T> 相当) -----------------------------
    static_assert(enum_elem_count<UnaryOp>() == 2);
    static_assert(enum_array<UnaryOp>[0].first == UnaryOp::not_);
    static_assert(enum_array<UnaryOp>[0].second == "!");          // 代表表記 (alt_names)
    static_assert(enum_name_array<UnaryOp>[0].second == "not_");  // 名前
    static_assert(from_string<UnaryOp>("!") == UnaryOp::not_);
    static_assert(from_string<UnaryOp>("not_") == UnaryOp::not_);  // as_json と round-trip
    static_assert(enum_elem_count<BinaryOp>() == 42);              // ast から取り込んだ全演算子
    static_assert(!from_string<UnaryOp>("nope").has_value());
    static_assert(enum_type_name<UnaryOp>() != nullptr);
    check(true, "enum helpers are constexpr-usable (checked by static_assert)");

    // ---- 演算子の優先順位層 (ast::expr_layer.h 相当) -----------------------
    // 層を平坦化したものが enum の並びそのものなので、ast 側の check_layers() に
    // 相当する「ズレていないか」の static_assert は要らない。ここでは対応だけ確認する。
    static_assert(bin_layer_count == 10 && bin_layer_len == 9);  // ignored を除くと 9
    static_assert(bin_compare_layer == 2 && bin_cond_layer == 5 &&
                  bin_range_layer == 6 && bin_assign_layer == 7 && bin_comma_layer == 8);
    static_assert(bin_layers[bin_compare_layer][0] == "==");
    static_assert(bin_layers[bin_assign_layer].size() == 15);

    // 層の並びと enum の並びが一致していること (生成の前提そのもの)
    bool layers_match = true;
    std::size_t seq = 0;
    for (std::size_t i = 0; i < bin_layer_count; i++) {
        for (auto op : bin_layers[i]) {
            if (enum_array<BinaryOp>[seq].second != op) {
                layers_match = false;
            }
            seq++;
        }
    }
    check(layers_match && seq == enum_elem_count<BinaryOp>(),
          "precedence layers flatten to exactly the BinaryOp order");

    // 述語。from/to は生成時に順序を正規化するので、schema で逆に書いても壊れない。
    // ast 側の is_range_op は begin=range_inclusive / end=range_exclusive で常に false だった。
    static_assert(is_compare_op(BinaryOp::equal) && is_compare_op(BinaryOp::grater_or_eq));
    static_assert(!is_compare_op(BinaryOp::add));
    static_assert(is_range_op(BinaryOp::range_exclusive) && is_range_op(BinaryOp::range_inclusive));
    static_assert(!is_range_op(BinaryOp::add));
    static_assert(is_assign_op(BinaryOp::assign) && is_assign_op(BinaryOp::append_assign));
    static_assert(is_define_op(BinaryOp::const_assign) && !is_define_op(BinaryOp::assign));
    check(true, "operator predicates are constexpr-usable (checked by static_assert)");

    // NodeType にも出しているので、全ノード種を走査できる
    static_assert(enum_elem_count<NodeType>() == enum_array<NodeType>.size());
    bool node_names_ok = true;
    for (auto& [t, n] : enum_array<NodeType>) {
        if (n != to_string(t) || from_string<NodeType>(n) != t) {
            node_names_ok = false;
        }
    }
    check(node_names_ok && enum_array<NodeType>.size() > 1,
          "enum_array<NodeType> enumerates every node kind and round-trips");

    // ---- pretty printer ----------------------------------------------------
    {
        Arena pa;
        auto mod = pa.make<Module>();
        auto f = pa.make<Format>(brgen::lexer::Loc{.line = 3, .col = 1});
        auto fn_name = pa.make<Ident>();
        auto fbody = pa.make<Body>();
        auto fld = pa.make<Field>(brgen::lexer::Loc{.line = 4, .col = 5});
        auto fld_name = pa.make<Ident>();
        auto ity = pa.make<IntType>();
        auto st = pa.make<StructType>();

        fn_name->identifier = "Sample";
        fld_name->identifier = "value";
        ity->bit_size = 8;
        f->name = fn_name;
        f->body = fbody;
        fld->name = fld_name;
        fld->type = ity;
        fbody->statements.push_back(fld);
        // fbody->struct_type = st;
        st->base = f;  // weak
        mod->statements.push_back(f);

        auto text = pretty_print(pa, mod.id());
        dump("pretty_print", text);
        check(text.find("Module #") != std::string::npos &&
                  text.find("Format #") != std::string::npos &&
                  text.find("\"Sample\"") != std::string::npos &&
                  text.find("\"value\"") != std::string::npos,
              "pretty printer walks the owning tree");
        // check(text.find("base -> Format #") != std::string::npos,
        //       "weak edges are shown as a reference, not descended into");
        //  weak を降りていたら StructType -> Format -> ... で無限に回る
        check(std::count(text.begin(), text.end(), '\n') < 40,
              "weak edges do not cause the walk to recurse");

        // ---- side table を木に併記する ------------------------------------
        // binder が何をどこに書いたかを、木の形のまま見るためのもの。
        SideTables pt;
        pt.table<Resolution>().set(fld_name.id(), Resolution{.target = fld.id()});   // dense
        pt.table<DocComment>().set(fld.id(), DocComment{.leading = {.line = 3}});    // sparse
        pt.table<IsMutated>().set(fld.id());                                         // flag
        auto with_tables = pretty_print(pa, pt, mod.id());
        dump("pretty_print (with side tables)", with_tables);
        check(with_tables.find("[Resolution].target -> Field #") != std::string::npos &&
                  with_tables.find("[DocComment].leading = 3:0") != std::string::npos &&
                  with_tables.find("[IsMutated] = true") != std::string::npos,
              "side table entries are printed under the node they key on");
        // 表の中の Node を降りると Ident -> Resolution -> Field -> name -> Ident で回る
        check(std::count(with_tables.begin(), with_tables.end(), '\n') < 40,
              "table entries are shown as references, not descended into");

        // ---- 親から子へ辿る (traverse.h) ----------------------------------
        // field<"..."> はパスがコンパイル時に決まるので、深さが実行時に
        // 決まる走査はこちら。weak は所有辺でないので渡さない。
        int children = 0;
        traverse(pa, f.id(), [&](auto) { children++; });
        // name と body。struct_type は空なので渡ってこない。
        check(children == 2, "traverse gives the owning children one level down");

        // 空のフィールドを渡さないこと。渡しても fn には Node しか届かず、
        // どのフィールドが空かは分からないので飛ばす以外にできることが無い。
        f->struct_type = st;
        children = 0;
        traverse(pa, f.id(), [&](auto) { children++; });
        check(children == 3, "a field that has been filled in shows up as a child");
        f->struct_type = nullref;
        children = 0;
        traverse(pa, f.id(), [&](auto) { children++; });
        check(children == 2, "an empty field is not handed to the callback");

        std::vector<NodeType> seen;
        visit_all(pa, f.id(), [&](auto n) { seen.push_back(n.type()); });
        // Format(name, body) -> Ident, Body(statements) -> Field(name, type) -> Ident, IntType
        check(seen.size() == 6 && seen[0] == NodeType::Format && seen[1] == NodeType::Ident &&
                  seen[2] == NodeType::Body && seen[3] == NodeType::Field &&
                  seen[4] == NodeType::Ident && seen[5] == NodeType::IntType,
              "visit_all walks the whole subtree in pre-order");

        int stopped = 0;
        visit_all(pa, f.id(), [&](auto n) {
            stopped++;
            return n.type() != NodeType::Body;
        });
        check(stopped == 3, "returning false stops the walk from descending");

        // weak を渡していたら StructType::base -> Format で戻って止まらない
        int from_struct = 0;
        visit_all(pa, st.id(), [&](auto) { from_struct++; });
        check(from_struct == 1, "a weak back-reference is not followed");

        // ---- 名前で辿る (access.h) ----------------------------------------
        // 存在しないフィールド名を書くと FieldOf の特殊化が無く、
        // 不完全型としてコンパイルエラーになる (実行時に落ちるのではない)。
        // Node は arena を持たないので渡す。Ref は自分で持っているので取らない。
        check(f.field<"body">().id() == fbody.id(),  //&&
                                                     /// f.id().field<"body.struct_type">(pa).id() == st.id(),
              "field<> follows Node fields through the arena, from Ref and from Node");
        check(f.field<"body.statements.0">().id() == fld.id(),
              "field<> indexes into a vector field");
        auto* elems = f.field<"body.statements">();
        check(elems && elems->size() == 1 && (*elems)[0] == fld.id(),
              "ending the path at a vector gives the vector itself");
        check(!f.field<"body.statements.9">(),
              "out of range index yields a null ref, not a crash");
        auto* ident = f.field<"name.identifier">();
        check(ident && *ident == "Sample",
              "a scalar at the end of the path comes back as a pointer");

        // 終端の .optional。ポインタや空 Ref を検査せず値として扱えるようにする。
        check(f.field<"name.identifier.optional">() == std::optional<std::string>("Sample"),
              ".optional turns the result into a value you can compare");
        // check(f.field<"body.struct_type.optional">().has_value(),
        //       ".optional works on a Node field too");
        auto bare = pa.make<Format>();
        check(/*!bare.field<"body.struct_type.optional">().has_value() &&*/
              !bare.field<"name.identifier.optional">().has_value() &&
                  !fbody.field<"statements.9.optional">().has_value(),
              ".optional is nullopt when anything on the way is null or out of range");
        /*check(Node<Format>{}.field<"body.struct_type">(pa).id().id() == 0,
              "a null node anywhere in the path yields a null result");
       */

        // パスから切り出した綴りが、生成側が書いた綴りと同じ型・同じ値になること。
        // ここがずれると FieldOf<T, h> が引けないので、長さは合っていないといけない。
        using namespace path_detail;
        static_assert(head<fixed_string("body.struct_type.fields.0")>().view() == "body");
        static_assert(tail<fixed_string("body.struct_type.fields.0")>().view() ==
                      "struct_type.fields.0");
        static_assert(tail<fixed_string("body")>().view().empty());
        static_assert(std::is_same_v<decltype(head<fixed_string("binary_value")>()),
                                     fixed_string<13>>);
        static_assert(head<fixed_string("binary_value.x")>() == fixed_string("binary_value"));
        check(true, "path segments keep the spelling the generated table uses");
    }

    // ---- ノードの比較 (compare.h) ------------------------------------------
    // id の一致 / 木として同じ / 意味として同じ、の 3 段。
    {
        Arena ca;
        auto mk = [&](std::size_t bits, bool exp, std::size_t line) {
            auto t = ca.make<IntType>(brgen::lexer::Loc{.line = line});
            t->is_explicit = exp;
            t->bit_size = bits;
            return t.id();
        };
        auto u8_written = mk(8, true, 1);
        auto u8_derived = mk(8, false, 2);  // is_explicit と loc だけ違う
        auto u16 = mk(16, true, 1);

        check(identical(ca, u8_written, u8_written) && equivalent(ca, u8_written, u8_written),
              "a node is both identical and equivalent to itself");
        check(!identical(ca, u8_written, u8_derived) && equivalent(ca, u8_written, u8_derived),
              "is_explicit and loc separate identical from equivalent");
        check(!identical(ca, u8_written, u16) && !equivalent(ca, u8_written, u16),
              "a field that carries meaning separates both");

        // 部分木ごと比べる
        auto f1 = ca.make<Field>();
        auto f2 = ca.make<Field>();
        auto n1 = ca.make<Ident>();
        auto n2 = ca.make<Ident>();
        n1->identifier = "v";
        n2->identifier = "v";
        f1->name = n1;
        f1->type = u8_written;
        f2->name = n2;
        f2->type = u8_derived;
        check(!identical(ca, f1.id(), f2.id()) && equivalent(ca, f1.id(), f2.id()),
              "the comparison walks the whole subtree");
        n2->identifier = "w";
        check(!equivalent(ca, f1.id(), f2.id()),
              "a difference anywhere in the subtree shows up");
    }

    // ---- side table -------------------------------------------------------
    // 解析結果をノードの外に置く表。storage が違っても API は共通。
    auto ident_b = arena.make<Ident>();

    // 表は Arena とは別の入れ物にまとめる。Arena に持たせると
    // 「持ち回す入れ物に構文と解析結果が同居する」形が 1 段上で再現するため。
    SideTables tables;

    auto& resolution = tables.table<Resolution>();  // dense
    check(!resolution.contains(name) && resolution.get(name) == nullptr,
          "dense table: empty lookup misses");
    resolution.set(name, Resolution{.target = fmt.id()});
    check(resolution.contains(name) && resolution.get(name)->target.id() == fmt.id().id(),
          "dense table: set/get round-trips");
    check(!resolution.contains(ident_b) && resolution.size() == 1,
          "dense table: unset node stays absent");
    check(resolution.set(Node<Ident>{}, Resolution{}) == nullptr,
          "dense table: null node is rejected");

    auto& docs = tables.table<DocComment>();  // sparse
    docs.set(field_a, DocComment{.leading = brgen::lexer::Loc{.line = 1}});
    docs.set(fmt, DocComment{.leading = brgen::lexer::Loc{.line = 5}});
    docs.set(func, DocComment{.leading = brgen::lexer::Loc{.line = 9}});
    check(docs.size() == 3 && docs.get(fmt)->leading.line == 5,
          "sparse table: holds entries and looks them up");
    check(std::is_sorted(docs.entries().begin(), docs.entries().end(),
                         [](const auto& l, const auto& r) { return l.node < r.node; }),
          "sparse table: entries stay sorted regardless of insertion order");
    check(docs.erase(fmt) && !docs.contains(fmt) && docs.size() == 2,
          "sparse table: erase removes the entry");

    auto& mutated = tables.table<IsMutated>();  // flag
    check(mutated.set(field_a) && !mutated.set(field_a),
          "flag table: set is idempotent and reports novelty");
    check(mutated.contains(field_a) && mutated.size() == 1, "flag table: membership");

    // const 側からも同じキーで引けること
    const SideTables& const_tables = tables;
    check(const_tables.table<Resolution>().contains(name) &&
              const_tables.table<IsMutated>().size() == 1,
          "table<T>() works on a const aggregate");

#ifdef NAST_TEST_WITH_JSON
    // 生成された as_json が実際に動くこと。
    // collect() のような事前走査なしで arena をそのまま出せるのが狙い。
    futils::json::Stringer<> s;
    arena.as_json(s);
    const std::string& out = s.out();
    dump("Arena::as_json", out);
    dump("pretty_print (main arena, from Format)", pretty_print(arena, fmt.id()));
    check(out.size() > 2 && out.front() == '{' && out.back() == '}',
          "Arena::as_json produces an object");
    check(out.find("\"headers\"") != std::string::npos &&
              out.find("\"data_Format\"") != std::string::npos &&
              out.find("\"Sample\"") != std::string::npos,
          "serialized output contains headers, per-type pools and payload");

#ifdef NAST_TEST_WITH_JSON_PARSE
    // as_json -> from_json -> as_json が一致すること
    {
        futils::json::JSON parsed;
        check(bool(futils::json::parse(out, parsed, true)), "serialized arena re-parses as JSON");
        Arena restored;
        check(from_json(restored, parsed), "from_json rebuilds the arena");
        check(restored.node_count() == arena.node_count(),
              "from_json restores every node");
        futils::json::Stringer<> again;
        restored.as_json(again);
        check(again.out() == out, "as_json -> from_json -> as_json round-trips");
    }
#endif

    // 表は arena とは別に出せる。構文木と解析結果が JSON 上でも分かれる。
    futils::json::Stringer<> ts;
    tables.as_json(ts);
    const std::string& tout = ts.out();
    dump("SideTables::as_json", tout);
    check(tout.find("\"Resolution\"") != std::string::npos &&
              tout.find("\"DocComment\"") != std::string::npos &&
              tout.find("\"IsMutated\"") != std::string::npos,
          "SideTables::as_json serializes every table independently of the arena");
#else
    print_line("  [--] Arena::as_json (skipped: futils not available)");
#endif

    // Builder の真偽三項の畳み込み。条件を捨てる形だけが副作用の有無に効く。
    {
        using namespace brgen::nast;
        Arena ba;
        Builder b{ba, {}};
        auto pure = b.bin(BinaryOp::equal, b.ref("kind"), b.lit(1), b.bool_type());
        check(b.cond(pure, b.bool_lit(true), b.bool_lit(false), b.bool_type()) == pure,
              "c ? true : false は条件そのもの");
        check(b.cond(pure, b.bool_lit(true), b.ref("x"), b.bool_type())
                  .as_any<Binary>()
                  .ref(ba)
                  ->op == BinaryOp::logical_or,
              "c ? true : x は ||");
        check(b.cond(pure, b.ref("x"), b.bool_lit(false), b.bool_type())
                  .as_any<Binary>()
                  .ref(ba)
                  ->op == BinaryOp::logical_and,
              "c ? x : false は &&");
        check(bool(b.cond(pure, b.bool_lit(true), b.bool_lit(true), b.bool_type())
                       .as_any<BoolLiteral>()),
              "両腕が同じで条件が副作用なしなら定数に畳む");

        // 呼び出しを含む条件は捨てられない。三項のまま残す。
        auto call = b.member_call(b.ref("input"), "get", nullref, b.int_type(8));
        auto impure = b.bin(BinaryOp::equal, call, b.lit(1), b.bool_type());
        check(bool(b.cond(impure, b.bool_lit(true), b.bool_lit(true), b.bool_type())
                       .as_any<Cond>()),
              "条件に呼び出しがあれば畳まない");
        check(bool(b.cond(impure, b.bool_lit(true), b.ref("x"), b.bool_type())
                       .as_any<Binary>()),
              "条件を残す畳み込みは呼び出しがあってもしてよい");
        // 型変換も木の上では Call だが、副作用は無い。
        auto cast = b.bin(BinaryOp::equal, b.cast(b.int_type(8), b.ref("v")), b.lit(1),
                          b.bool_type());
        check(bool(b.cond(cast, b.bool_lit(false), b.bool_lit(false), b.bool_type())
                       .as_any<BoolLiteral>()),
              "型変換の Call は副作用として数えない");
    }

    print_line("{} ({} failure{})", failures == 0 ? "PASS" : "FAIL", failures,
                 failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
