/*license*/
// Go のバックエンド。
//
//   ./nast2go -i input.bgn            標準出力へ
//   ./nast2go -i input.bgn -o out.go
//   ./nast2go -i input.bgn --unhandled error   未対応で止める
//
// 木を辿るのは共通側 (backend/entry.hpp)。ここに書くのは knob の設定だけ。
// 何も差し込まなければ全ノードが {{Unhandled node: ...}} として出るので、
// 出力を見ながら埋めていく。
//
// 今の段階: 型の宣言と、固定長の並びだけの format の Decode/Encode。
// 条件付き / union / 非オクテット幅 / stream はまだ。
#include "../../backend/entry.hpp"

#include <format>

using namespace brgen::nast;
using namespace brgen::nast::backend;

struct GoConfig {
    static constexpr auto lang_name = "go";
    static constexpr auto file_extension = ".go";

    std::string_view package = "main";

    // 生成した本体が io を使ったか。Go は使わない import がコンパイルエラー
    // なので、子を辿り終えてから import 行を決める。
    bool uses_io = false;

    void bind(futils::cmdline::option::Context& ctx) {
        ctx.VarString<true>(&package, "package", "package name of the generated file", "NAME");
    }

    // Go は gofmt がタブ。桁の数え方も変わるので IndentStyle::tab() で揃える。
    IndentStyle indent_style() {
        return IndentStyle::tab();
    }
};

namespace {

    // Go の識別子は先頭が大文字なら公開。生成物は外から使うので上げる。
    std::string exported(std::string_view name) {
        if (name.empty()) {
            return std::string(name);
        }
        std::string out(name);
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
        return out;
    }

    template <class C>
    expected<std::string> type_name(C& c, Node<Type> t);

    // 配列の長さ。定数なら [N]T、そうでなければ []T にする。
    template <class C>
    std::optional<std::uint64_t> const_length(C& c, Node<Expr> len) {
        if (!len) {
            return std::nullopt;
        }
        // 定数畳み込みの結果は側の表にある。式を読み直さない。
        if (auto* v = c.tables().template table<ConstantValue>().get(len)) {
            if (v->kind == EvalKind::integer && !v->is_negative) {
                return v->integer;
            }
        }
        return std::nullopt;
    }

    template <class C>
    expected<std::string> type_name(C& c, Node<Type> t) {
        if (!t) {
            return unexpect_loc_error(t, "no type");
        }
        auto& a = c.arena();
        t = strip_wrappers(a, t);
        if (auto i = t.template as<IntType>()) {
            auto r = i.ref(a);
            // bit_size が 8/16/32/64 でないもの (u3 など) はまだ扱わない。
            auto bits = r->bit_size;
            if (bits != 8 && bits != 16 && bits != 32 && bits != 64) {
                return unexpect_loc_error(t, "int of {} bits does not map to a Go type yet", bits);
            }
            return std::format("{}int{}", r->is_signed ? "" : "u", bits);
        }
        if (auto f = t.template as<FloatType>()) {
            return std::format("float{}", f.ref(a)->bit_size);
        }
        if (t.template as<BoolType>()) {
            return std::string("bool");
        }
        if (auto arr = t.template as<ArrayType>()) {
            auto r = arr.ref(a);
            MAYBE(elem, type_name(c, r->element_type));
            if (auto n = const_length(c, r->length)) {
                return std::format("[{}]{}", *n, elem);
            }
            return std::format("[]{}", elem);
        }
        if (auto s = t.template as<StructType>()) {
            // 名前は元の宣言から取る。struct_type は宣言への弱い戻り辺。
            auto base = s.ref(a)->base;
            if (auto fmt = base.template as<Format>()) {
                return exported(ident_text(a, fmt.ref(a)->name));
            }
        }
        if (auto e = t.template as<EnumType>()) {
            return exported(ident_text(a, e.ref(a)->base.ref(a)->name));
        }
        return unexpect_loc_error(t, "no Go type for {}", to_string(t.type()));
    }

    // --- 式 ---------------------------------------------------------------
    // 長さの式 (`[header.length-8]u8`) に要るぶんだけ。名前が何を指している
    // かは Resolution (名前解決の結果) から取る。木の形だけでは決まらない。

    template <class C>
    expected<std::string> emit_expr(C& c, Node<Expr> e);

    // 参照先が同じ format のフィールドなら t.X になる。それ以外 (定数や
    // 他の format のもの) はまだ扱わない。
    template <class C>
    expected<std::string> emit_reference(C& c, Node<Reference> r) {
        auto& a = c.arena();
        auto name = r.ref(a)->name;
        auto* res = c.tables().template table<Resolution>().get(name);
        if (!res) {
            return unexpect_loc_error(r, "unresolved name {}", ident_text(a, name));
        }
        auto target = res->target;
        if (target.template as<Field>() || target.template as<VariableDefinition>()) {
            return std::format("t.{}", exported(name_of(a, target)));
        }
        return unexpect_loc_error(r, "reference to {} is not handled yet", to_string(target.type()));
    }

    template <class C>
    expected<std::string> emit_expr(C& c, Node<Expr> e) {
        auto& a = c.arena();
        if (!e) {
            return unexpect_loc_error(e, "no expression");
        }
        // 畳み込めているならそれを使う。式を組み直しても同じ値にしかならない。
        if (auto* v = c.tables().template table<ConstantValue>().get(e)) {
            if (v->kind == EvalKind::integer) {
                return std::format("{}{}", v->is_negative ? "-" : "", v->integer);
            }
        }
        if (auto lit = e.template as<IntLiteral>()) {
            return std::string(lit.ref(a)->value);
        }
        if (auto lit = e.template as<StrLiteral>()) {
            // 綴りのまま (引用符込み) 持っている。Go の文字列リテラルとは
            // 逃がし方が違うので、そのまま出せるのは覚書に載せるときだけ。
            return std::string(lit.ref(a)->value);
        }
        if (auto ref = e.template as<Reference>()) {
            return emit_reference(c, ref);
        }
        if (auto ma = e.template as<MemberAccess>()) {
            auto r = ma.ref(a);
            MAYBE(base, emit_expr(c, r->base));
            return std::format("{}.{}", base, exported(ident_text(a, r->member)));
        }
        if (auto bin = e.template as<Binary>()) {
            auto r = bin.ref(a);
            auto op = to_string(r->op, 1);  // alt=1 が記号
            MAYBE(lhs, emit_expr(c, r->left));
            MAYBE(rhs, emit_expr(c, r->right));
            // Go の算術は型が揃っていないと通らない。長さは int に寄せる。
            return std::format("(int({}) {} int({}))", lhs, op, rhs);
        }
        return unexpect_loc_error(e, "no Go expression for {}", to_string(e.type()));
    }

    // --- codec ------------------------------------------------------------
    // 今のところ「先頭から順に固定長で並んでいるだけ」の format を対象にする。
    // それ以外 (条件付き / union / 可変長が途中に来る) はここで断る。

    bool is_big_endian(Endian e) {
        // 未指定は big。.bgn の既定であり、config.endian は SpecifyOrder で
        // 明示されたときだけ変わる。
        return e != Endian::little;
    }

    // 1 つの整数をバイト列から組む式。
    std::string read_int_expr(std::string_view buf, std::string_view off, std::size_t bytes,
                              bool big, std::string_view go_type) {
        if (bytes == 1) {
            return std::format("{}({}[{}])", go_type, buf, off);
        }
        std::string out;
        for (std::size_t i = 0; i < bytes; i++) {
            auto byte_index = big ? i : bytes - 1 - i;
            auto shift = (bytes - 1 - i) * 8;
            if (i) {
                out += " | ";
            }
            out += std::format("{}({}[{}+{}])<<{}", go_type, buf, off, byte_index, shift);
        }
        return out;
    }

    std::string write_int_args(std::string_view expr, std::size_t bytes, bool big) {
        std::string out;
        for (std::size_t i = 0; i < bytes; i++) {
            auto shift = big ? (bytes - 1 - i) * 8 : i * 8;
            if (i) {
                out += ", ";
            }
            out += shift ? std::format("byte({}>>{})", expr, shift) : std::format("byte({})", expr);
        }
        return out;
    }

    template <class C>
    expected<CodeWriter> decode_field(C& c, std::string_view target, Node<Type> t);

    template <class C>
    expected<CodeWriter> encode_field(C& c, std::string_view target, Node<Type> t);

    template <class C>
    expected<CodeWriter> decode_field(C& c, std::string_view target, Node<Type> t) {
        auto& a = c.arena();
        t = strip_wrappers(a, t);
        CodeWriter w;
        if (auto i = t.template as<IntType>()) {
            auto r = i.ref(a);
            auto bytes = r->bit_size / 8;
            if (r->bit_size % 8 != 0) {
                return unexpect_loc_error(t, "int of {} bits needs the bit reader", r->bit_size);
            }
            MAYBE(go, type_name(c, t));
            c.lang_config().uses_io = true;
            w.writeln(std::format("if len(data) < o+{} {{", bytes));
            {
                auto s = w.indent_scope();
                w.writeln("return o, io.ErrUnexpectedEOF");
            }
            w.writeln("}");
            w.writeln(std::format("{} = {}", target,
                                  read_int_expr("data", "o", bytes, is_big_endian(r->endian), go)));
            w.writeln(std::format("o += {}", bytes));
            return w;
        }
        if (auto arr = t.template as<ArrayType>()) {
            auto r = arr.ref(a);
            // 長さが「無い」(末尾まで) のと「実行時に決まる」のは別物。
            // 一緒にすると、計算した長さの配列を黙って末尾まで読む出力になる。
            std::string count;
            if (!r->length) {
                MAYBE(elem, type_name(c, r->element_type));
                if (elem != "uint8") {
                    return unexpect_loc_error(t, "unbounded array of {} is not handled yet", elem);
                }
                w.writeln(std::format("{} = append([]uint8(nil), data[o:]...)", target));
                w.writeln("o = len(data)");
                return w;
            }
            if (auto n = const_length(c, r->length)) {
                count = std::format("{}", *n);
            }
            else {
                MAYBE(len, emit_expr(c, r->length));
                count = std::format("int({})", len);
                // 長さが実行時なら Go の型は [] なので、先に確保する。
                MAYBE(elem, type_name(c, r->element_type));
                w.writeln(std::format("{} = make([]{}, {})", target, elem, count));
                count = std::format("len({})", target);
            }
            w.writeln(std::format("for i := 0; i < {}; i++ {{", count));
            {
                auto s = w.indent_scope();
                MAYBE(inner, decode_field(c, std::format("{}[i]", target), r->element_type));
                w.write(std::move(inner));
            }
            w.writeln("}");
            return w;
        }
        if (auto s = t.template as<StructType>()) {
            if (auto fmt = s.ref(a)->base.template as<Format>()) {
                w.writeln(std::format("if n, err := {}.Decode(data[o:]); err != nil {{", target));
                {
                    auto sc = w.indent_scope();
                    w.writeln("return o + n, err");
                }
                w.writeln("} else {");
                {
                    auto sc = w.indent_scope();
                    w.writeln("o += n");
                }
                w.writeln("}");
                return w;
            }
        }
        return unexpect_loc_error(t, "no decode for {}", to_string(t.type()));
    }

    template <class C>
    expected<CodeWriter> encode_field(C& c, std::string_view target, Node<Type> t) {
        auto& a = c.arena();
        t = strip_wrappers(a, t);
        CodeWriter w;
        if (auto i = t.template as<IntType>()) {
            auto r = i.ref(a);
            if (r->bit_size % 8 != 0) {
                return unexpect_loc_error(t, "int of {} bits needs the bit writer", r->bit_size);
            }
            w.writeln(std::format("out = append(out, {})",
                                  write_int_args(target, r->bit_size / 8, is_big_endian(r->endian))));
            return w;
        }
        if (auto arr = t.template as<ArrayType>()) {
            auto r = arr.ref(a);
            if (auto n = const_length(c, r->length)) {
                w.writeln(std::format("for i := 0; i < {}; i++ {{", *n));
            }
            else {
                // 書く側は持っている分だけ出す。長さの式との一致は
                // assert が見る話なので、ここでは決めない。
                w.writeln(std::format("for i := range {} {{", target));
            }
            {
                auto s = w.indent_scope();
                MAYBE(inner, encode_field(c, std::format("{}[i]", target), r->element_type));
                w.write(std::move(inner));
            }
            w.writeln("}");
            return w;
        }
        if (auto s = t.template as<StructType>()) {
            if (auto fmt = s.ref(a)->base.template as<Format>()) {
                w.writeln(std::format("if b, err := {}.Encode(); err != nil {{", target));
                {
                    auto sc = w.indent_scope();
                    w.writeln("return nil, err");
                }
                w.writeln("} else {");
                {
                    auto sc = w.indent_scope();
                    w.writeln("out = append(out, b...)");
                }
                w.writeln("}");
                return w;
            }
        }
        return unexpect_loc_error(t, "no encode for {}", to_string(t.type()));
    }

    // format 1 つぶんの Decode/Encode。並びは FormatState (解析結果) から取る。
    // 木を自前で歩くと、inner struct を平らにしたところで元の並びを見失う。
    template <class C>
    expected<CodeWriter> codec_methods(C& c, Node<Format> n) {
        auto& a = c.arena();
        auto name = exported(ident_text(a, n.ref(a)->name));
        auto* state = c.tables().template table<FormatState>().get(n);
        if (!state) {
            return unexpect_loc_error(n, "no format state");
        }
        if (state->decode_kind == FormatKind::custom || state->encode_kind == FormatKind::custom) {
            return unexpect_loc_error(n, "custom encode/decode is not handled yet");
        }

        CodeWriter dec, enc;
        for (auto& f : state->fields) {
            auto r = f.ref(a);
            auto fname = ident_text(a, r->name);
            if (fname.empty()) {
                return unexpect_loc_error(f, "anonymous field is not handled yet");
            }
            auto target = std::format("t.{}", exported(fname));
            MAYBE(d, decode_field(c, target, r->type));
            dec.write(std::move(d));
            MAYBE(e, encode_field(c, target, r->type));
            enc.write(std::move(e));
        }

        CodeWriter w;
        w.writeln(std::format("func (t *{}) Decode(data []byte) (int, error) {{", name));
        {
            auto s = w.indent_scope();
            w.writeln("o := 0");
            w.write(std::move(dec));
            w.writeln("return o, nil");
        }
        w.writeln("}");
        w.writeln();
        w.writeln(std::format("func (t *{}) Encode() ([]byte, error) {{", name));
        {
            auto s = w.indent_scope();
            w.writeln("var out []byte");
            w.write(std::move(enc));
            w.writeln("return out, nil");
        }
        w.writeln("}");
        return w;
    }

}  // namespace

NAST_BACKEND_ENTRY(GoConfig) {
    knobs.bind_Module(ctx, [](auto& c, Node<Module> n) -> expected<CodeWriter> {
        // import 行は子を辿り終えてからでないと決められない (Go は使わない
        // import がエラー)。本体を先に組んで、頭を後から付ける。
        CodeWriter body;
        for (auto& s : n.ref(c.arena())->statements) {
            MAYBE(part, c.visit(s));
            body.writeln();
            body.write(std::move(part));
        }
        CodeWriter w;
        w.writeln("// Code generated by nast2go. DO NOT EDIT.");
        w.writeln(std::format("package {}", c.lang_config().package));
        if (c.lang_config().uses_io) {
            w.writeln();
            w.writeln("import \"io\"");
        }
        w.write(std::move(body));
        return w;
    });

    knobs.bind_Format(ctx, [](auto& c, Node<Format> n) -> expected<CodeWriter> {
        auto& a = c.arena();
        auto r = n.ref(a);
        CodeWriter w;
        w.writeln(std::format("type {} struct {{", exported(ident_text(a, r->name))));
        {
            auto indent = w.indent_scope();
            for (auto& s : r->body.ref(a)->statements) {
                MAYBE(part, c.visit(s));
                w.write(std::move(part));
            }
        }
        w.writeln("}");
        MAYBE(methods, codec_methods(c, n));
        w.writeln();
        w.write(std::move(methods));
        return w;
    });

    knobs.bind_Metadata(ctx, [](auto& c, Node<Metadata> n) -> expected<CodeWriter> {
        // config.* は生成物には効かない覚書。コメントとして残す。
        auto& a = c.arena();
        auto r = n.ref(a);
        CodeWriter w;
        std::string args;
        for (auto& arg : r->arguments.ref(a)->arguments) {
            MAYBE(text, emit_expr(c, arg.ref(a)->value));
            if (!args.empty()) {
                args += ", ";
            }
            args += text;
        }
        w.writeln(std::format("// {}: {}", r->name, args));
        return w;
    });

    knobs.bind_Field(ctx, [](auto& c, Node<Field> n) -> expected<CodeWriter> {
        auto& a = c.arena();
        auto r = n.ref(a);
        // 名前のないフィールド (アンカーやパディング) は構造体には出さない。
        auto name = ident_text(a, r->name);
        if (name.empty()) {
            return CodeWriter{};
        }
        MAYBE(type, type_name(c, r->type));
        CodeWriter w;
        w.writeln(std::format("{} {}", exported(name), type));
        return w;
    });

    return {};
}
