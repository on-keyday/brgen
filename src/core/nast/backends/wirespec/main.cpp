/*license*/
// wirespec のバックエンド。
//
//   ./nast2wirespec -i input.bgn            標準出力へ
//   ./nast2wirespec -i input.bgn -o out.wirespec
//   ./nast2wirespec -i input.bgn --unhandled error   未対応で止める
//
// 木を辿るのは共通側 (backend/entry.hpp)。ここに書くのは knob の設定だけ。
// 何も差し込まなければ全ノードが {{Unhandled node: ...}} として出るので、
// 出力を見ながら埋めていく。
#include "../../backend/entry.hpp"
#include "../../parse/unparse.h"
#include "../../node/access.h"

using namespace brgen::nast;
using namespace brgen::nast::backend;

struct WirespecConfig {
    static constexpr auto lang_name = "wirespec";
    static constexpr auto file_extension = ".wspec";
    // 言語ごとの状態はここに置く (ctx.lang_config() で取れる)。

    // 言語ごとの追加フラグが要るならここで登録する。--show-flags にも出る。
    // void bind(futils::cmdline::option::Context& ctx) {
    //     ctx.VarString<true>(&package, "package", "package name", "NAME");
    // }

    // 字下げを変えるなら (Go のようにタブなら IndentStyle::tab())。
    // IndentStyle indent_style() { return IndentStyle::tab(); }

    // 文字列ではなく自前の構造を組み上げたいときは戻り値の型を変える。
    // その場合 finish で出力の中身にする。
    // using result_type = MyTree;
    // std::string finish(MyTree&& t) { ... }
};

NAST_BACKEND_ENTRY(WirespecConfig) {
    knobs.bind_Module(ctx, [](LangContext& c, Node<Module> n) -> expected<CodeWriter> {
        CodeWriter w;
        for (auto& s : n.ref(c.arena())->statements) {
            MAYBE(part, c.visit(s));
            w.write(std::move(part));
        }
        return w;
    });
    knobs.bind_Format(ctx, [](LangContext& c, Node<Format> n) -> expected<CodeWriter> {
        CodeWriter w;
        w.writeln("packet ", name_of(c.arena(), n), " {");
        auto indent = w.indent_scope();
        for (auto& field : n.ref(c.arena())->body.ref(c.arena())->statements) {
            MAYBE(part, c.visit(field));
            w.write(std::move(part));
        }
        indent.execute();
        w.writeln("}");
        return w;
    });
    knobs.bind_Field(ctx, [](LangContext& c, Node<Field> n) -> expected<CodeWriter> {
        auto name = name_of(c.arena(), n);
        CodeWriter w;
        auto type = n.field<"type">(c.arena());
        MAYBE(t, c.visit(type.id()));
        w.writeln(name, ": ", std::move(t), ",");
        return w;
    });
    knobs.none_Metadata();
    knobs.bind_IntType(ctx, [](LangContext& c, Node<IntType> n) -> expected<CodeWriter> {
        return unparse_writer(c.arena(), n);
    });
    knobs.bind_ArrayType(ctx, [](LangContext& c, Node<ArrayType> n) -> expected<CodeWriter> {
        auto elem = n.ref(c)->element_type.ref(c);
        MAYBE(len, c.visit(n.ref(c)->length));
        auto& t = c.tables();
        if (elem.id().type() == NodeType::IntType &&
            t.table<TypeSize>().get(elem)->bits == 8) {
            return CODE("bytes[", len, "]");
        }
        MAYBE(elem_v, c.visit(elem.id()));
        return CODE("[", elem_v, "; ", len, "]");
    });
    knobs.bind_IdentType(ctx, [](LangContext& c, Node<IdentType> n) -> expected<CodeWriter> {
        return CODE(n.ref(c)->ident.ref(c)->identifier);
    });
    knobs.bind_Binary(ctx, [](LangContext& c, Node<Binary> n) -> expected<CodeWriter> {
        MAYBE(left, c.visit(n.ref(c)->left));
        MAYBE(right, c.visit(n.ref(c)->right));
        return CODE("(", left, " ", to_string(n.ref(c)->op, 1), " ", right, ")");
    });
    //
    // デフォルトの挙動を一部だけ変えたいときは、その場で c.visit_default(n)
    // を呼んで残りを任せる。
    return {};
}
