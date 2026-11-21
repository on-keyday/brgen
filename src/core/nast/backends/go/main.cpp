/*license*/
// Go のバックエンド。
//
//   ./nast2go -i input.bgn            標準出力へ
//   ./nast2go input1.bgn input2.bgn   まとめて (--survey と併用)
//
// 公開 API は ebm2go と同じ形 (unictest の Go の runner がそのまま使える):
//
//   DecodeSlice(buf []byte, off *int) error    スライスから。[]byte の field は切り出すだけ (コピーしない)
//   Read(r io.Reader) error                    reader から
//   Append(out []byte) ([]byte, error)         out に足す
//   Decode / Encode / Write                    上の 3 つの薄いラッパ
//
// 本体は 1 つ (backend/codec.hpp の Walker が format の Body を歩く) で、
// 入出力の口ごとに展開する。口ごとに本体を物理的に複製するのは、1 つの本体を
// 口の分岐で共有すると、interface に渡すバッファがエスケープ解析で
// スライスの口までヒープに逃がしてしまうため (2026-11-17 に udp で測った)。
//
// 読み書きの形は lowering から選ぶ:
//   1/2/4/8 バイトの整数   scalar_shape のデータ → binary.BigEndian / LittleEndian / NativeEndian
//   それ以外の幅            lower_field_* の文 (combine_int / split_int) — Walker の既定
//   []byte                  スライスの切り出し / append
//   それ以外の配列          lower_field_* の文 (要素のループ) — Walker の既定
//   平らなビット群          bit_group_shape のデータ → 1 つの整数を読んで shift / mask
//   分岐を跨ぐビット群      lower_routed_bit_* の文 — Walker の既定
//
// 組めない format は飛ばし、それを使う format も落とす。
#include "../../backend/codec.hpp"
#include "../../backend/entry.hpp"
#include "../../lowering/available.hpp"
#include "../../lowering/predicate.hpp"

#include <fnet/util/base64.h>
#include "../../parse/unparse.h"

#include <format>
#include <map>
#include <set>

using namespace brgen::nast;
using namespace brgen::nast::backend;

// 値の無い expected 用の MAYBE (backend/common.hpp の MAYBE と同じ形)。
#define MAYBE_VOID(x, v)                                                 \
    auto x##_nast_maybe = (v);                                           \
    if (!x##_nast_maybe) {                                               \
        return brgen::nast::unexpect(std::move(x##_nast_maybe.error())); \
    }

struct GoConfig {
    static constexpr auto lang_name = "go";
    static constexpr auto file_extension = ".go";

    std::string_view package = "main";

    void bind(futils::cmdline::option::Context& ctx) {
        ctx.VarString<true>(&package, "package", "package name of the generated file", "NAME");
    }

    IndentStyle indent_style() {
        return IndentStyle::tab();
    }
};

namespace {

    // --- 名前 --------------------------------------------------------------

    // snake_case を CamelCase に (src_port -> SrcPort)。ebm2go と同じ綴り。
    // Go の識別子に使えない字 (ASCII 以外) は X と 16 進のバイト列にする。
    std::string sanitize(std::string_view name) {
        std::string out;
        for (auto ch : name) {
            auto u = static_cast<unsigned char>(ch);
            if (u < 0x80) {
                out += ch;
                continue;
            }
            out += std::format("X{:02X}", u);
        }
        return out;
    }

    std::string camel(std::string_view raw) {
        auto name = sanitize(raw);
        std::string out;
        bool up = true;
        for (auto ch : name) {
            if (ch == '_') {
                up = true;
                continue;
            }
            out += up ? static_cast<char>(std::toupper(static_cast<unsigned char>(ch))) : ch;
            up = false;
        }
        if (out.empty()) {
            out = "X";
        }
        return out;
    }

    // 局所変数の名前。Go の予約語と、生成した関数の引数の名前を避ける。
    std::string local_name(std::string_view raw) {
        auto sanitized = sanitize(raw);
        std::string_view name = sanitized;
        static const std::set<std::string_view> reserved = {
            "break", "case", "chan", "const", "continue", "default", "defer", "else", "fallthrough", "for",
            "func", "go", "goto", "if", "import", "interface", "map", "package", "range", "return",
            "select", "struct", "switch", "type", "var", "len", "cap", "make", "new", "append", "copy",
            "string", "byte", "int", "uint", "error", "true", "false", "nil", "t", "r", "w", "buf", "off",
            "out", "err"};
        if (reserved.contains(name)) {
            return std::string(name) + "_";
        }
        return std::string(name);
    }

    std::string go_string(std::string_view s) {
        std::string out = "\"";
        for (auto ch : s) {
            if (ch == '"' || ch == '\\') {
                out += '\\';
            }
            if (ch == '\n') {
                out += "\\n";
                continue;
            }
            out += ch;
        }
        out += '"';
        return out;
    }

    // 検査に落ちたときのエラー。
    std::string assert_error(std::string_view what) {
        return std::format("errors.New({})", go_string("assertion failed: " + std::string(what)));
    }

    // バイト列を Go の文字列リテラルに。印字できない字は \xNN で逃がす。
    std::string go_bytes(std::string_view s) {
        std::string out = "\"";
        for (auto ch : s) {
            auto u = static_cast<unsigned char>(ch);
            if (u >= 0x20 && u < 0x7f && ch != '"' && ch != '\\') {
                out += ch;
                continue;
            }
            out += std::format("\\x{:02x}", u);
        }
        out += '"';
        return out;
    }

    // --- 型 ----------------------------------------------------------------

    std::size_t int_bits_of(std::string_view go) {
        auto p = go.find_first_of("0123456789");
        std::size_t n = 0;
        for (auto c : go.substr(p == std::string_view::npos ? go.size() : p)) {
            n = n * 10 + (c - '0');
        }
        return n;
    }

    bool is_go_int(std::string_view go) {
        return go.starts_with("uint") || go.starts_with("int");
    }

    // 式の Go の型。型の無いリテラルは空、分からなければ unknown_type。
    constexpr std::string_view unknown_type = "?";

    bool known(std::string_view t) {
        return !t.empty() && t != unknown_type;
    }

    std::string wider(std::string_view l, std::string_view r) {
        if (!is_go_int(l) || !is_go_int(r)) {
            return std::string(l);
        }
        auto bits = std::max(int_bits_of(l), int_bits_of(r));
        bool sign = l.starts_with("int") || r.starts_with("int");
        return std::format("{}int{}", sign ? "" : "u", bits);
    }

    // 型の無いリテラル (have が空) はそのまま。型が分からないものは want に
    // 包む (同じ型なら何も起きない)。
    std::string convert(std::string_view text, std::string_view have, std::string_view want) {
        if (!known(want) || have.empty() || have == want) {
            return std::string(text);
        }
        return std::format("{}({})", want, text);
    }

    bool is_compare(BinaryOp op) {
        switch (op) {
            case BinaryOp::equal:
            case BinaryOp::not_equal:
            case BinaryOp::less:
            case BinaryOp::less_or_eq:
            case BinaryOp::grater:
            case BinaryOp::grater_or_eq:
                return true;
            default:
                return false;
        }
    }

    // 入出力の口。本体はこの 3 つに展開する。
    enum class Mode : std::uint8_t {
        slice,   // DecodeSlice(buf []byte, off *int) error
        reader,  // Read(r io.Reader) error
        append,  // Append(out []byte) ([]byte, error)
    };

    // --- 式と文 ------------------------------------------------------------
    //
    // lowering が組んだ nast のノード (と .bgn に書かれた式) を Go に綴る。
    // Go は暗黙の型変換が無いので、型の違う演算と代入には変換を挟む。

    struct GoPrinter {
        Arena& a;
        SideTables& tables;
        lowering::Context lc;
        // struct のメンバ。.bgn の名前から Go の型へ。
        std::map<std::string, std::string> members;
        // 局所変数。名前から Go の型へ (分かるものだけ)。
        std::map<std::string, std::string> locals;
        // 文の前に出した式 (input.get()) の代わりに読む名前。
        std::map<std::uint32_t, std::string> replaced;
        // WithReceiver が積むレシーバの綴り。空なら self (t)。
        std::vector<std::string> receivers;
        Mode mode = Mode::slice;

        // エラーを返す文。Append は値も返す。
        std::string fail(std::string_view err) const {
            return mode == Mode::append ? std::format("return nil, {}", err) : std::format("return {}", err);
        }

        expected<std::string> type_name(Node<Type> t);
        std::string type_of(Node<Expr> e);
        expected<std::string> expr(Node<Expr> e);
        expected<void> stmt(CodeWriter& w, Node<Statement> s);
        expected<void> body(CodeWriter& w, Node<Body> b);
    };

    expected<std::string> GoPrinter::type_name(Node<Type> t) {
        if (!t) {
            return unexpect_loc_error(t, "missing type");
        }
        t = strip_wrappers(a, t);
        if (auto i = t.as_any<IntType>()) {
            auto r = i.ref(a);
            auto bits = r->bit_size;
            if (bits == 0 || bits > 64) {
                return unexpect_loc_error(t, "{}-bit integer has no Go type", bits);
            }
            std::size_t go_bits = bits <= 8 ? 8 : bits <= 16 ? 16 : bits <= 32 ? 32 : 64;
            return std::format("{}int{}", r->is_signed ? "" : "u", go_bits);
        }
        if (auto f = t.as_any<FloatType>()) {
            return std::format("float{}", f.ref(a)->bit_size);
        }
        if (t.as_any<BoolType>()) {
            return std::string("bool");
        }
        if (auto arr = t.as_any<ArrayType>()) {
            auto r = arr.ref(a);
            MAYBE(elem, type_name(r->element_type));
            if (r->length && !r->length.as_any<Range>()) {
                if (auto n = const_uint(a, tables, r->length)) {
                    return std::format("[{}]{}", *n, elem);
                }
            }
            return std::format("[]{}", elem);
        }
        if (auto s = t.as_any<StructType>()) {
            if (auto f = s.ref(a)->base.as_any<Format>()) {
                return camel(name_of(a, f));
            }
            if (auto st = s.ref(a)->base.as_any<State>()) {
                return camel(name_of(a, st));
            }
        }
        if (auto e = t.as_any<EnumType>()) {
            return camel(name_of(a, e.ref(a)->base));
        }
        if (auto bytes = lowering::literal_bytes(lc, t)) {
            return std::format("[{}]uint8", bytes->size());  // 文字列の期待値は固定長のバイト列
        }
        return unexpect_loc_error(t, "no Go type for {} yet", to_string(t.type()));
    }

    std::string GoPrinter::type_of(Node<Expr> e) {
        if (!e) {
            return std::string(unknown_type);
        }
        e = strip_paren(a, e);
        if (e.as_any<IntLiteral>() || e.as_any<CharLiteral>()) {
            return {};
        }
        if (auto r = e.as_any<Reference>()) {
            auto name = ident_text(a, r.ref(a)->name);
            auto* res = tables.table<Resolution>().get(r.ref(a)->name);
            if (res && res->target.as_any<Field>()) {
                if (auto it = members.find(std::string(name)); it != members.end()) {
                    return it->second;
                }
            }
            if (auto it = locals.find(local_name(name)); it != locals.end()) {
                return it->second;
            }
        }
        if (auto m = e.as_any<MemberAccess>()) {
            if (strip_paren(a, m.ref(a)->base).as_any<Self>()) {
                if (auto it = members.find(std::string(ident_text(a, m.ref(a)->member))); it != members.end()) {
                    return it->second;
                }
            }
        }
        if (auto w = e.as_any<WithReceiver>()) {
            return type_of(w.ref(a)->expr);
        }
        if (auto ix = e.as_any<Index>()) {
            auto base = type_of(ix.ref(a)->base);
            if (auto p = base.find(']'); p != std::string::npos) {
                return base.substr(p + 1);
            }
        }
        if (auto t = e.ref(a)->type) {
            if (strip_wrappers(a, t).as_any<IntLiteralType>()) {
                return {};
            }
            if (auto n = type_name(t)) {
                return *n;
            }
        }
        return std::string(unknown_type);
    }

    expected<std::string> GoPrinter::expr(Node<Expr> e) {
        if (!e) {
            return unexpect_loc_error(e, "missing expression");
        }
        if (auto it = replaced.find(strip_paren(a, e).id()); it != replaced.end()) {
            return it->second;
        }
        if (auto p = e.as_any<Paren>()) {
            MAYBE(inner, expr(p.ref(a)->expr));
            return "(" + inner + ")";
        }
        if (auto lit = e.as_any<IntLiteral>()) {
            if (auto v = const_uint(a, tables, e)) {
                return std::format("{}", *v);
            }
            return std::string(lit.ref(a)->value);
        }
        if (auto lit = e.as_any<BoolLiteral>()) {
            return std::string(lit.ref(a)->value ? "true" : "false");
        }
        if (auto lit = e.as_any<CharLiteral>()) {
            return std::format("{}", lit.ref(a)->code);
        }
        if (e.as_any<Self>()) {
            return receivers.empty() ? std::string("t") : receivers.back();
        }
        if (auto w = e.as_any<WithReceiver>()) {
            // レシーバを付け替えて中の式を綴る (`available(lab.pointer)` の lab)。
            MAYBE(recv, expr(w.ref(a)->receiver));
            receivers.push_back(recv);
            auto r = expr(w.ref(a)->expr);
            receivers.pop_back();
            return r;
        }
        if (auto r = e.as_any<Reference>()) {
            auto name = ident_text(a, r.ref(a)->name);
            auto* res = tables.table<Resolution>().get(r.ref(a)->name);
            if (!res) {
                return local_name(name);  // lowering が合成した名前
            }
            auto target = res->target;
            if (target.as_any<Field>()) {
                if (!receivers.empty()) {
                    return receivers.back() + "." + camel(name);
                }
                if (members.contains(std::string(name))) {
                    return "t." + camel(name);
                }
                return unexpect_loc_error(e, "field {} does not belong to this format", name);
            }
            if (target.as_any<VariableDefinition>() || target.as_any<Parameter>()) {
                return local_name(name);
            }
            if (target.as_any<StateVariable>()) {
                return local_name(name);  // 受け取った state (ポインタ)
            }
            return unexpect_loc_error(e, "name referring to {}", to_string(target.type()));
        }
        if (auto m = e.as_any<MemberAccess>()) {
            auto mr = m.ref(a);
            auto base = strip_paren(a, mr->base);
            auto member = ident_text(a, mr->member);
            if (auto br = base.as_any<Reference>()) {
                auto* res = tables.table<Resolution>().get(br.ref(a)->name);
                if (res && res->target.as_any<Enum>()) {
                    return camel(name_of(a, res->target)) + camel(member);
                }
            }
            if (base.as_any<SpecialLiteral>()) {
                return unexpect_loc_error(e, "member {} of input / output", member);
            }
            MAYBE(b, expr(mr->base));
            if (member == "length" && type_of(mr->base).starts_with("[")) {
                auto t = type_of(e);
                return std::format("{}(len({}))", known(t) ? t : "uint64", b);
            }
            return b + "." + camel(member);
        }
        if (auto bin = e.as_any<Binary>()) {
            auto br = bin.ref(a);
            if (br->op == BinaryOp::equal || br->op == BinaryOp::not_equal) {
                if (auto lowered = lowering::lower_range_compare(lc, bin)) {
                    return expr(lowered);
                }
            }
            MAYBE(l, expr(br->left));
            MAYBE(r, expr(br->right));
            auto lt = type_of(br->left);
            auto rt = type_of(br->right);
            std::string op = to_string(br->op, 1);
            if (br->op == BinaryOp::right_arithmetic_shift) {
                op = ">>";
            }
            else if (br->op == BinaryOp::left_arithmetic_shift) {
                op = "<<";
            }
            if (br->op == BinaryOp::logical_and || br->op == BinaryOp::logical_or) {
                return l + " " + op + " " + r;
            }
            if (is_compare(br->op)) {
                if (known(lt) && known(rt) && lt != rt) {
                    auto w = wider(lt, rt);
                    return convert(l, lt, w) + " " + op + " " + convert(r, rt, w);
                }
                if (known(lt) != known(rt) && !lt.empty() && !rt.empty()) {
                    auto w = known(lt) ? lt : rt;
                    return convert(l, lt, w) + " " + op + " " + convert(r, rt, w);
                }
                return l + " " + op + " " + r;
            }
            auto want = type_of(e);
            if (!known(want)) {
                want = known(lt) ? lt : rt;
            }
            bool shift = br->op == BinaryOp::left_logical_shift || br->op == BinaryOp::right_logical_shift ||
                         br->op == BinaryOp::right_arithmetic_shift || br->op == BinaryOp::left_arithmetic_shift;
            // 型の無いリテラルを左に置いたシフトは、右が定数でないと型が決まらない。
            auto left = lt.empty() && shift && known(want) ? std::format("{}({})", want, l) : convert(l, lt, want);
            auto right = shift ? r : convert(r, rt, want);
            return left + " " + op + " " + right;
        }
        if (auto u = e.as_any<Unary>()) {
            MAYBE(inner, expr(u.ref(a)->target));
            if (u.ref(a)->op == UnaryOp::minus_sign) {
                return "-" + inner;
            }
            return (type_of(u.ref(a)->target) == "bool" ? "!" : "^") + inner;
        }
        if (auto c = e.as_any<Cast>()) {
            auto args = c.ref(a)->arguments.ref(a);
            if (!args || args->arguments.size() != 1) {
                return unexpect_loc_error(e, "conversion with other than one argument");
            }
            auto value = args->arguments[0].ref(a)->value;
            MAYBE(to, type_name(c.ref(a)->type));
            MAYBE(v, expr(value));
            auto have = type_of(value);
            if (to == "bool" && is_go_int(have)) {
                return "(" + v + " != 0)";
            }
            if (have == "bool" && is_go_int(to)) {
                return std::format("func() {} {{ if {} {{ return 1 }}; return 0 }}()", to, v);
            }
            return to + "(" + v + ")";
        }
        if (auto av = e.as_any<Available>()) {
            auto lowered = lowering::lower_available(lc, av);
            if (!lowered) {
                return unexpect_loc_error(e, "cannot lower available()");
            }
            return expr(lowered);
        }
        if (auto sz = e.as_any<Sizeof>()) {
            // 幅の決まった型か値のバイト数。TypeSize から。
            auto target = strip_paren(a, sz.ref(a)->target);
            Node<Type> t;
            if (auto r = target.as_any<Reference>()) {
                auto* res = tables.table<Resolution>().get(r.ref(a)->name);
                if (auto f = res ? res->target.as_any<Format>() : Node<Format>{}) {
                    t = f.ref(a)->struct_type;
                }
            }
            if (!t && target) {
                t = target.ref(a)->type;
            }
            auto* size = t ? tables.table<TypeSize>().get(t) : nullptr;
            if (!size || size->kind != SizeKind::fixed || size->bits % 8 != 0) {
                return unexpect_loc_error(e, "sizeof of a type without a fixed byte size");
            }
            return std::format("{}", size->bits / 8);
        }
        if (auto ix = e.as_any<Index>()) {
            MAYBE(b, expr(ix.ref(a)->base));
            MAYBE(i, expr(ix.ref(a)->index));
            return b + "[" + i + "]";
        }
        if (auto c = e.as_any<Cond>()) {
            // Go に式の分岐が無いので、即時の関数で包む。
            auto cr = c.ref(a);
            MAYBE(cond, expr(cr->cond));
            MAYBE(then, expr(cr->then));
            MAYBE(els, expr(cr->els));
            auto t = type_of(e);
            if (!known(t)) {
                t = type_of(cr->then);
            }
            if (!known(t)) {
                t = type_of(cr->els);
            }
            if (!known(t)) {
                t = "uint64";
            }
            return std::format("func() {} {{ if {} {{ return {} }}; return {} }}()", t, cond,
                               convert(then, type_of(cr->then), t), convert(els, type_of(cr->els), t));
        }
        return unexpect_loc_error(e, "{} is not supported as a Go expression yet", to_string(e.type()));
    }

    expected<void> GoPrinter::body(CodeWriter& w, Node<Body> b) {
        if (!b) {
            return unexpect_loc_error(b, "missing body");
        }
        for (auto& s : b.ref(a)->statements) {
            MAYBE_VOID(r, stmt(w, s));
        }
        return {};
    }

    expected<void> GoPrinter::stmt(CodeWriter& w, Node<Statement> s) {
        if (auto as = s.as_any<Assign>()) {
            auto r = as.ref(a);
            MAYBE(lhs, expr(r->assignee));
            MAYBE(rhs, expr(r->value));
            std::string op = r->op == BinaryOp::assign ? "=" : to_string(r->op, 1);
            w.writeln(lhs, " ", op, " ", convert(rhs, type_of(r->value), type_of(r->assignee)));
            return {};
        }
        if (auto def = s.as_any<VariableDefinition>()) {
            auto r = def.ref(a);
            auto name = local_name(ident_text(a, r->name));
            MAYBE(v, expr(r->value));
            auto t = type_of(r->value);
            if (t.empty()) {
                t = "uint64";  // 型の無いリテラル
                v = "uint64(" + v + ")";
            }
            if (known(t)) {
                locals[name] = t;
            }
            else {
                locals.erase(name);
            }
            w.writeln(name, " := ", v);
            w.writeln("_ = ", name);
            return {};
        }
        if (auto as = s.as_any<Assert>()) {
            auto cond = as.ref(a)->expr;
            MAYBE(c, expr(cond));
            w.writeln("if !(", c, ") {");
            {
                auto sc = w.indent_scope();
                w.writeln(fail(assert_error((unparse_node(a, cond)))));
            }
            w.writeln("}");
            return {};
        }
        if (auto loop = s.as_any<Loop>()) {
            auto r = loop.ref(a);
            auto init = r->init.as_any<VariableDefinition>();
            auto step = r->step.as_any<Assign>();
            if (!init || !step || !r->condition) {
                return unexpect_loc_error(s, "only counting loops are supported");
            }
            auto name = local_name(ident_text(a, init.ref(a)->name));
            MAYBE(v, expr(init.ref(a)->value));
            locals[name] = "uint64";
            MAYBE(cond, expr(r->condition));
            MAYBE(sl, expr(step.ref(a)->assignee));
            MAYBE(sr, expr(step.ref(a)->value));
            w.writeln("for ", name, " := uint64(", v, "); ", cond, "; ", sl, " = ", sr, " {");
            {
                auto sc = w.indent_scope();
                MAYBE_VOID(b, body(w, r->body));
            }
            w.writeln("}");
            return {};
        }
        if (auto ee = s.as_any<ExplicitError>()) {
            std::string msg = "error";
            if (auto lit = ee.ref(a)->message) {
                std::string decoded;
                if (futils::base64::decode(lit.ref(a)->binary_value, decoded)) {
                    msg = decoded;
                }
            }
            w.writeln(fail(std::format("errors.New({})", go_string(msg))));
            return {};
        }
        return unexpect_loc_error(s, "{} is not supported as a Go statement yet", to_string(s.type()));
    }

    // --- codec -------------------------------------------------------------

    // その format が decode / encode で受け取る state 変数 (Requirements から、id 順)。
    std::vector<Node<StateVariable>> state_params(SideTables& tables, Node<Format> fmt, bool decode) {
        std::vector<Node<StateVariable>> out;
        auto* req = fmt ? tables.table<Requirements>().get(fmt) : nullptr;
        if (!req) {
            return out;
        }
        for (auto* v : {decode ? &req->decode_state_read : &req->encode_state_read,
                        decode ? &req->decode_state_write : &req->encode_state_write}) {
            for (auto sv : *v) {
                if (std::find(out.begin(), out.end(), sv) == out.end()) {
                    out.push_back(sv);
                }
            }
        }
        std::ranges::sort(out, [](auto l, auto r) { return l.id() < r.id(); });
        return out;
    }

    // `, HeaderState` (呼ぶ側の引数) / `, HeaderState *FrameHeaderState` (受ける側)。
    // state 変数の引数名。型名 (CamelCase) と衝突しないよう snake_case のまま。
    std::string state_name(Arena& a, Node<StateVariable> sv) {
        return local_name(name_of(a, sv));
    }

    std::string state_args(Arena& a, const std::vector<Node<StateVariable>>& v) {
        std::string out;
        for (auto sv : v) {
            out += ", " + state_name(a, sv);
        }
        return out;
    }

    // 本体の最後が return か、抜け出しの無い無限ループなら、その後ろは届かない。
    bool ends_terminal(Arena& a, Node<Body> b) {
        if (!b || b.ref(a)->statements.empty()) {
            return false;
        }
        auto last = b.ref(a)->statements.back();
        if (last.as_any<Return>()) {
            return true;
        }
        auto loop = last.as_any<Loop>();
        if (!loop || loop.ref(a)->condition) {
            return false;
        }
        // このループを抜ける break (入れ子のループの中のものは数えない)。
        bool breaks = false;
        visit_all(a, loop.ref(a)->body, [&](NodeAny n) {
            if (n.as_any<Loop>() || n.as_any<RangeLoop>()) {
                return false;
            }
            if (n.as_any<Break>()) {
                breaks = true;
            }
            return !breaks;
        });
        return !breaks;
    }

    Node<Format> format_of(Arena& a, Node<Type> t) {
        auto s = strip_wrappers(a, t).as_any<StructType>();
        return s ? s.ref(a)->base.as_any<Format>() : Node<Format>{};
    }

    using Walker = codec::Walker<struct GoCodec>;

    // 本体を 1 つの口に展開する。
    struct GoCodec : Walker {
        GoPrinter& p;
        CodeWriter& w;
        Mode mode;
        IndentStack ind;

        GoCodec(GoPrinter& p, CodeWriter& w, Mode m)
            : Walker(p.a, p.tables), p(p), w(w), mode(m), ind(w) {}

        // ---- 小道具 ----

        void block(std::string_view head, std::string_view line) {
            w.writeln(head, " {");
            {
                auto s = w.indent_scope();
                w.writeln(line);
            }
            w.writeln("}");
        }

        // スライスの口: 残りが n バイト以上あること。
        void need(std::string_view n) {
            block(std::format("if len(buf)-*off < {}", n), "return " + short_err());
        }

        // reader の口: dst を埋める。
        void read_full(std::string_view dst) {
            block(std::format("if _, err := io.ReadFull(r, {}); err != nil", dst), "return " + read_err());
        }

        // 足りないときのエラー。どの field かは walker が決める (ebm2go と同じ文言)。
        std::string short_err() {
            return std::format("errors.New({})", go_string("not enough data to read for field \"" + where() + "\""));
        }

        // reader の読み込みに失敗したときのエラー。元のエラー (io.EOF など) は
        // errors.Is で辿れるよう %w で包む。
        std::string read_err() {
            return std::format("fmt.Errorf({}, err)", go_string("read field \"" + where() + "\": %w"));
        }

        // reader から n バイト読み、dst ([]byte) に入れる。n は入力から来るので、
        // 一度に確保せず 64 KiB ずつ伸ばす。パッケージ直下に routine を置かない
        // (生成ファイルを同じパッケージに複数置けるように) ので、その場に展開する。
        void read_n(std::string_view dst, std::string_view n) {
            w.writeln("var ", dst, " []byte");
            w.writeln("{");
            {
                auto s = w.indent_scope();
                w.writeln("want := ", n);
                block("if want < 0", "return " + short_err());
                w.writeln("for len(", dst, ") < want {");
                {
                    auto s2 = w.indent_scope();
                    w.writeln("k := want - len(", dst, ")");
                    block("if k > 1<<16", "k = 1 << 16");
                    w.writeln("old := len(", dst, ")");
                    w.writeln(dst, " = append(", dst, ", make([]byte, k)...)");
                    read_full(std::string(dst) + "[old:]");
                }
                w.writeln("}");
            }
            w.writeln("}");
        }

        // dst[i] = src[i] ^ key[i % len(key)]。key が空ならそのまま写す (同じ領域なら何もしない)。
        void xor_into(std::string_view dst, std::string_view src, std::string_view key) {
            w.writeln("if k := ", key, "; len(k) > 0 {");
            {
                auto s = w.indent_scope();
                w.writeln("for i := range ", src, " {");
                {
                    auto s2 = w.indent_scope();
                    w.writeln(dst, "[i] = ", src, "[i] ^ k[i%len(k)]");
                }
                w.writeln("}");
            }
            if (dst != src) {
                w.writeln("} else {");
                {
                    auto s = w.indent_scope();
                    w.writeln("copy(", dst, ", ", src, ")");
                }
            }
            w.writeln("}");
        }

        std::string ref_name(Node<Expr> e) {
            auto r = p.expr(e);
            return r ? *r : std::string("?");
        }

        // ---- Walker の既定の無い口 ----

        expected<void> emit_statement(Node<Statement> s) {
            return p.stmt(w, s);
        }

        expected<void> emit_body(Node<Body> b) {
            return p.body(w, b);
        }

        Node<Expr> temp_target(Node<Field> f) {
            auto name = derived_name("pad", f);
            auto t = p.type_name(f.ref(a)->type);
            w.writeln("var ", name, " ", t ? *t : "uint8");
            w.writeln("_ = ", name);
            if (t) {
                p.locals[name] = *t;
            }
            return builder(f).ref(name, f.ref(a)->type);
        }

        // encode で名前の無い field の値。期待値があればそれ、無ければ 0。
        Node<Expr> preset_target(Node<Field> f) {
            auto ref = temp_target(f);
            if (auto ex = field_args(a, f).expected) {
                if (auto v = p.expr(ex)) {
                    auto name = derived_name("pad", f);
                    w.writeln(name, " = ", convert(*v, p.type_of(ex), p.locals[name]));
                }
            }
            return ref;
        }

        // decode: nbytes (式) か fixed (定数) バイトを手元に揃え、その名前を返す。
        expected<Node<Expr>> acquire(Node<Field> f, Node<Expr> nbytes, std::size_t fixed) {
            auto name = derived_name("in", f);
            std::string n;
            if (nbytes) {
                MAYBE(text, p.expr(nbytes));
                n = derived_name("n", f);
                w.writeln(n, " := int(", text, ")");
            }
            if (mode == Mode::slice) {
                auto count = nbytes ? n : std::to_string(fixed);
                if (nbytes) {
                    block(std::format("if {} < 0 || len(buf)-*off < {}", n, n), "return " + short_err());
                }
                else {
                    need(count);
                }
                w.writeln(name, " := buf[*off : *off+", count, "]");
                w.writeln("*off += ", count);
                p.locals[name] = "[]uint8";
            }
            else if (!nbytes) {
                w.writeln("var ", name, " [", std::to_string(fixed), "]byte");
                read_full(name + "[:]");
                p.locals[name] = std::format("[{}]uint8", fixed);
            }
            else {
                read_n(name, n);
                p.locals[name] = "[]uint8";
            }
            return builder(f).ref(name);
        }

        // encode: 書くバイトの置き場所。
        expected<Node<Expr>> declare_out(Node<Field> f, Node<Expr> nbytes, std::size_t fixed) {
            auto name = derived_name("out", f);
            if (nbytes) {
                MAYBE(text, p.expr(nbytes));
                w.writeln(name, " := make([]byte, int(", text, "))");
                p.locals[name] = "[]uint8";
            }
            else {
                w.writeln("var ", name, " [", std::to_string(fixed), "]byte");
                p.locals[name] = std::format("[{}]uint8", fixed);
            }
            return builder(f).ref(name);
        }

        expected<void> flush(Node<Expr> buf, std::size_t from, Node<Expr> count) {
            MAYBE(b, p.expr(buf));
            MAYBE(c, p.expr(count));
            w.writeln("out = append(out, ", b, "[", std::to_string(from), ":", std::to_string(from), "+", c, "]...)");
            return {};
        }

        expected<void> declare_group_buffer(Node<Expr> buf, std::size_t max_bytes) {
            auto name = ref_name(buf);
            w.writeln("var ", name, " [", std::to_string(max_bytes), "]byte");
            w.writeln("_ = ", name);
            p.locals[name] = std::format("[{}]uint8", max_bytes);
            return {};
        }

        expected<void> fetch_into(Node<Expr> buf, std::size_t from, std::size_t n) {
            auto name = ref_name(buf);
            auto range = std::format("{}[{}:{}]", name, from, from + n);
            if (mode == Mode::slice) {
                need(std::to_string(n));
                w.writeln("copy(", range, ", buf[*off:*off+", std::to_string(n), "])");
                w.writeln("*off += ", std::to_string(n));
                return {};
            }
            read_full(range);
            return {};
        }

        expected<void> allocate(Node<Expr> target, Node<ArrayType> arr, Node<Expr> count) {
            MAYBE(t, p.expr(target));
            MAYBE(tn, p.type_name(arr));
            MAYBE(n, p.expr(count));
            w.writeln(t, " = make(", tn, ", int(", n, "))");
            return {};
        }

        expected<void> check_count(Node<Expr> target, Node<Expr> count) {
            MAYBE(t, p.expr(target));
            MAYBE(n, p.expr(count));
            block(std::format("if len({}) != int({})", t, n),
                  p.fail(assert_error(("length of " + t + " matches " + unparse_node(a, count)))));
            return {};
        }

        // 入れ子 format を読む / 書く。呼ぶ先が state を受け取るなら、自分が
        // 受け取ったものをそのまま渡す (Requirements が親へ伝播させてある)。
        void call_nested(std::string_view target, Node<Format> callee) {
            auto states = state_args(a, state_params(tables, callee, decode));
            auto impl = states.empty() ? "" : "Impl";
            switch (mode) {
                case Mode::slice:
                    block(std::format("if err := {}.{}(buf, off{}); err != nil", target, states.empty() ? "DecodeSlice" : "decodeSliceImpl", states), "return err");
                    break;
                case Mode::reader:
                    block(std::format("if err := {}.{}(r{}); err != nil", target, states.empty() ? "Read" : "readImpl", states), "return err");
                    break;
                case Mode::append:
                    block(std::format("if out, err = {}.{}(out{}); err != nil", target, states.empty() ? "Append" : "appendImpl", states), "return nil, err");
                    break;
            }
            (void)impl;
        }

        expected<void> nested(Node<Field> f, Node<Expr> target) {
            if (!target) {
                target = temp_target(f);
            }
            MAYBE(t, p.expr(target));
            call_nested(t, format_of(a, f.ref(a)->type));
            return {};
        }

        expected<void> nested_array(Node<Field> f, Node<ArrayType> arr, Node<Expr> target) {
            if (!target) {
                return unexpect_loc_error(f, "unnamed array of formats");
            }
            MAYBE(t, p.expr(target));
            auto len = arr.ref(a)->length;
            bool fixed = const_uint(a, tables, len).has_value();
            if (fixed) {
                w.writeln("for i := range ", t, " {");
                {
                    auto s = w.indent_scope();
                    call_nested(t + "[i]", format_of(a, arr.ref(a)->element_type));
                }
                w.writeln("}");
                return {};
            }
            MAYBE(n, p.expr(len));
            if (!decode) {
                MAYBE_VOID(r, check_count(target, len));
                w.writeln("for i := range ", t, " {");
                {
                    auto s = w.indent_scope();
                    call_nested(t + "[i]", format_of(a, arr.ref(a)->element_type));
                }
                w.writeln("}");
                return {};
            }
            // 入力から来た数で先に確保しない。読めた分だけ足す。
            MAYBE(elem, p.type_name(arr.ref(a)->element_type));
            w.writeln(t, " = nil");
            w.writeln("for i := 0; i < int(", n, "); i++ {");
            {
                auto s = w.indent_scope();
                w.writeln("var e ", elem);
                call_nested("e", format_of(a, arr.ref(a)->element_type));
                w.writeln(t, " = append(", t, ", e)");
            }
            w.writeln("}");
            return {};
        }

        // `input = input.subrange(n)`: n バイトの部分範囲を作り、inner をその中で
        // スライスの口として綴る。reader の口は n バイトを読んでからスライスに
        // する。append の口は書いた長さが n と合うかを確かめる。
        template <class Inner>
        expected<void> subrange(Node<Field> f, Node<Expr> len, Inner&& inner) {
            MAYBE(n, p.expr(len));
            auto nv = derived_name("sub", f);
            auto sub_off = derived_name("subOff", f);
            w.writeln("{");
            ind.enter();
            if (mode == Mode::append) {
                auto start = derived_name("start", f);
                w.writeln(start, " := len(out)");
                MAYBE_VOID(r, inner());
                block(std::format("if len(out)-{} != int({})", start, n),
                      p.fail(assert_error("length of " + std::string(name_of(a, f)) + " matches " + unparse_node(a, len))));
                ind.leave();
                w.writeln("}");
                return {};
            }
            w.writeln(nv, " := int(", n, ")");
            if (mode == Mode::slice) {
                block(std::format("if {} < 0 || len(buf)-*off < {}", nv, nv), "return " + short_err());
                auto outer = derived_name("outer", f);
                w.writeln(outer, " := off");
                w.writeln("buf := buf[*off : *off+", nv, "]");
                w.writeln(sub_off, " := 0");
                w.writeln("off := &", sub_off);
                w.writeln("_, _ = buf, off");
                MAYBE_VOID(r, inner());
                w.writeln("*", outer, " += ", nv);
            }
            else {
                auto bytes_name = derived_name("subBytes", f);
                read_n(bytes_name, nv);
                w.writeln("buf := ", bytes_name);
                w.writeln(sub_off, " := 0");
                w.writeln("off := &", sub_off);
                w.writeln("_, _ = buf, off");
                mode = Mode::slice;
                auto r = inner();
                mode = Mode::reader;
                if (!r) {
                    return r;
                }
            }
            ind.leave();
            w.writeln("}");
            return {};
        }

        // 長さの無い配列で、要素がバイトでないもの (入れ子 format / 整数)。
        // 要素を 1 つずつ読んで足す。どこで止まるかは follow で決まる。
        expected<void> elements_until_end(Node<Field> f, Node<ArrayType> arr, Node<Expr> target,
                                          const lowering::FieldFollow& follow) {
            using Kind = lowering::FieldFollow::Kind;
            auto elem_type = arr.ref(a)->element_type;
            auto elem = strip_wrappers(a, elem_type);
            bool is_format = elem.as_any<StructType>() && elem.as_any<StructType>().ref(a)->base.as_any<Format>();
            auto shape = is_format ? std::nullopt : lowering::scalar_shape(lc, f, elem_type);
            if (!is_format && !shape) {
                return unexpect_loc_error(f, "array without length of {} elements", to_string(elem.type()));
            }
            MAYBE(elem_name, p.type_name(elem_type));
            auto b = builder(f);
            // 要素 1 つを e に読む / 書く。
            auto one = [&](std::string_view e, Node<Expr> e_ref) -> expected<void> {
                if (is_format) {
                    call_nested(e, format_of(a, elem_type));
                    return {};
                }
                return scalar(f, *shape, e_ref);
            };
            if (!target) {
                return unexpect_loc_error(f, "unnamed array without length");
            }
            MAYBE(t, p.expr(target));
            if (!decode) {
                auto idx = b.index(target, b.ref("i"), elem_type);
                MAYBE(e, p.expr(idx));
                w.writeln("for i := range ", t, " {");
                {
                    auto s = w.indent_scope();
                    MAYBE_VOID(r, one(e, idx));
                }
                w.writeln("}");
                return {};
            }
            auto e_name = derived_name("e", f);
            auto e_ref = b.ref(e_name, elem_type);
            p.locals[e_name] = elem_name;
            auto body = [&]() -> expected<void> {
                w.writeln("var ", e_name, " ", elem_name);
                MAYBE_VOID(r, one(e_name, e_ref));
                w.writeln(t, " = append(", t, ", ", e_name, ")");
                return {};
            };
            w.writeln(t, " = nil");
            if (follow.kind == Kind::end) {
                if (mode == Mode::slice) {
                    w.writeln("for *off < len(buf) {");
                    ind.enter();
                    MAYBE_VOID(r, body());
                    ind.leave();
                    w.writeln("}");
                    return {};
                }
                // reader: 1 バイト覗けたら要素が始まっている。覗いた分は要素の
                // 読み込みに戻す。要素の途中で尽きたらその読み込みが失敗を返す。
                auto peek = derived_name("peek", f);
                w.writeln("for {");
                ind.enter();
                w.writeln("var ", peek, " [1]byte");
                w.writeln("if _, err := io.ReadFull(r, ", peek, "[:]); err != nil {");
                {
                    auto s = w.indent_scope();
                    block("if err == io.EOF", "break");
                    w.writeln("return err");
                }
                w.writeln("}");
                w.writeln("r := io.MultiReader(bytes.NewReader(", peek, "[:]), r)");
                w.writeln("_ = r");
                MAYBE_VOID(r, body());
                ind.leave();
                w.writeln("}");
                return {};
            }
            if (follow.kind == Kind::fixed && follow.eventual == Kind::end && follow.tail_bits && *follow.tail_bits % 8 == 0) {
                auto tail = std::to_string(*follow.tail_bits / 8);
                if (mode == Mode::slice) {
                    w.writeln("for len(buf)-*off > ", tail, " {");
                    ind.enter();
                    MAYBE_VOID(r, body());
                    ind.leave();
                    w.writeln("}");
                    return {};
                }
                // reader: 全部読み、後ろの固定幅を残りの field 用の reader に戻して、
                // 要素はスライスの口で読む。
                auto rest = derived_name("rest", f);
                auto sub_off = derived_name("subOff", f);
                w.writeln("{");
                ind.enter();
                w.writeln(rest, ", err := io.ReadAll(r)");
                block("if err != nil", "return err");
                block(std::format("if len({}) < {}", rest, tail), "return " + short_err());
                w.writeln("r = bytes.NewReader(", rest, "[len(", rest, ")-", tail, ":])");
                w.writeln("buf := ", rest, "[:len(", rest, ")-", tail, "]");
                w.writeln(sub_off, " := 0");
                w.writeln("off := &", sub_off);
                mode = Mode::slice;
                w.writeln("for *off < len(buf) {");
                ind.enter();
                auto r = body();
                ind.leave();
                w.writeln("}");
                mode = Mode::reader;
                if (!r) {
                    return r;
                }
                ind.leave();
                w.writeln("}");
                return {};
            }
            return unexpect_loc_error(f, "array without length followed by a variable-width or constant field");
        }

        // 長さの無いバイト列。どこまで読むかは後ろに何が来るか (follow) で決まる:
        //   end                      入力の終わりまで
        //   fixed で eventual が end  残りから後ろの固定幅を除いたところまで
        //   constant                  終端の文字列が現れるところまで
        expected<void> array_until_end(Node<Field> f, Node<ArrayType> arr, Node<Expr> target,
                                       const lowering::FieldFollow& follow) {
            using Kind = lowering::FieldFollow::Kind;
            if (field_args(a, f).positional || named_args(f)) {
                return unexpect_loc_error(f, "array without length with arguments");
            }
            auto it = strip_wrappers(a, arr.ref(a)->element_type).as_any<IntType>();
            if (!it || it.ref(a)->bit_size != 8 || it.ref(a)->is_signed) {
                return elements_until_end(f, arr, target, follow);
            }
            if (!decode) {
                if (target) {
                    MAYBE(t, p.expr(target));
                    w.writeln("out = append(out, ", t, "...)");
                }
                return {};
            }
            MAYBE(t, p.expr(target));
            if (follow.kind == Kind::end) {
                if (mode == Mode::slice) {
                    w.writeln(t, " = buf[*off:]");
                    w.writeln("*off = len(buf)");
                    return {};
                }
                w.writeln("{");
                {
                    auto s = w.indent_scope();
                    w.writeln("rest, err := io.ReadAll(r)");
                    block("if err != nil", "return err");
                    w.writeln(t, " = rest");
                }
                w.writeln("}");
                return {};
            }
            if (follow.kind == Kind::fixed && follow.eventual == Kind::end && follow.tail_bits && *follow.tail_bits % 8 == 0) {
                auto tail = std::to_string(*follow.tail_bits / 8);
                if (mode == Mode::slice) {
                    auto n = derived_name("n", f);
                    w.writeln(n, " := len(buf) - *off - ", tail);
                    block(std::format("if {} < 0", n), "return " + short_err());
                    w.writeln(t, " = buf[*off : *off+", n, "]");
                    w.writeln("*off += ", n);
                    return {};
                }
                // reader には残りの長さが無い。全部読んで、後ろの固定幅を残りの
                // field 用の reader に戻す。
                w.writeln("{");
                {
                    auto s = w.indent_scope();
                    w.writeln("rest, err := io.ReadAll(r)");
                    block("if err != nil", "return err");
                    block(std::format("if len(rest) < {}", tail), "return " + short_err());
                    w.writeln(t, " = rest[:len(rest)-", tail, "]");
                    w.writeln("r = bytes.NewReader(rest[len(rest)-", tail, ":])");
                }
                w.writeln("}");
                return {};
            }
            if (follow.kind == Kind::constant && follow.next) {
                auto term = lowering::literal_bytes(lc, follow.next.ref(a)->type);
                if (!term || term->empty()) {
                    return unexpect_loc_error(f, "terminator is not a byte string");
                }
                if (mode == Mode::slice) {
                    // 終端の手前まで切り出す。終端そのものは後ろの field が読んで比べる。
                    auto i = derived_name("n", f);
                    w.writeln(i, " := bytes.Index(buf[*off:], []byte(", go_bytes(*term), "))");
                    block(std::format("if {} < 0", i), "return " + short_err());
                    w.writeln(t, " = buf[*off : *off+", i, "]");
                    w.writeln("*off += ", i);
                    return {};
                }
                if (term->size() != 1) {
                    return unexpect_loc_error(f, "multi-byte terminator needs peek on a reader");
                }
                // 1 バイトずつ読み、終端に当たったらそれを終端の field に入れて止まる。
                auto sentinel = std::to_string(static_cast<unsigned char>((*term)[0]));
                w.writeln(t, " = nil");
                w.writeln("for {");
                {
                    auto s = w.indent_scope();
                    w.writeln("var b [1]byte");
                    read_full("b[:]");
                    w.writeln("if b[0] == ", sentinel, " {");
                    {
                        auto s2 = w.indent_scope();
                        if (!name_of(a, follow.next).empty()) {
                            MAYBE(term_target, p.expr(lowering::field_ref(lc, follow.next)));
                            w.writeln(term_target, "[0] = b[0]");
                        }
                        w.writeln("break");
                    }
                    w.writeln("}");
                    w.writeln(t, " = append(", t, ", b[0])");
                }
                w.writeln("}");
                consumed.insert(follow.next.id());
                return {};
            }
            return unexpect_loc_error(f, "array without length followed by a variable-width field");
        }

        // 文字列の期待値。読んで比べる / そのまま書く。
        expected<void> literal_field(Node<Field> f, const std::string& bytes, Node<Expr> target) {
            auto lit = go_bytes(bytes);
            if (!decode) {
                w.writeln("out = append(out, ", lit, "...)");
                return {};
            }
            auto src = take(f, bytes.size());
            block(std::format("if string({}) != {}", src, lit),
                  p.fail(assert_error((std::string(name_of(a, f)) + " == " + lit))));
            if (target && !name_of(a, f).empty()) {
                MAYBE(t, p.expr(target));
                w.writeln("copy(", t, "[:], ", src, ")");
            }
            return {};
        }

        expected<void> branch_arm(Node<Expr> cond, bool first) {
            if (!first) {
                ind.leave();
            }
            if (cond) {
                MAYBE(c, p.expr(cond));
                w.writeln(first ? "if " : "} else if ", c, " {");
            }
            else {
                w.writeln(first ? "{" : "} else {");
            }
            ind.enter();
            return {};
        }

        expected<void> branch_end() {
            ind.leave();
            w.writeln("}");
            return {};
        }

        // ---- 手書きの fn の口 ----

        expected<void> stream_get(Node<Call> call) {
            auto name = derived_name("g", call);
            auto args = call.ref(a)->arguments.ref(a);
            Node<Type> fmt_type;
            if (args && !args->arguments.empty()) {
                auto t = strip_wrappers(a, call.ref(a)->type);
                auto st = t.as_any<StructType>();
                auto it = t.as_any<IntType>();
                if (it && it.ref(a)->bit_size == 8) {
                    // input.get(u8) / input.get(i8) は引数なしと同じ 1 バイト
                }
                else if (!st || !st.ref(a)->base.as_any<Format>()) {
                    return unexpect_loc_error(call, "input.get with a type argument other than a format or a byte");
                }
                else {
                    fmt_type = t;
                }
            }
            if (fmt_type) {
                MAYBE(tn, p.type_name(fmt_type));
                w.writeln("var ", name, " ", tn);
                call_nested(name, format_of(a, fmt_type));
                p.locals[name] = tn;
            }
            else if (mode == Mode::slice) {
                need("1");
                w.writeln(name, " := buf[*off]");
                w.writeln("*off++");
                p.locals[name] = "uint8";
            }
            else {
                auto tmp = name + "b";
                w.writeln("var ", tmp, " [1]byte");
                read_full(tmp + "[:]");
                w.writeln(name, " := ", tmp, "[0]");
                p.locals[name] = "uint8";
            }
            w.writeln("_ = ", name);
            p.replaced[call.id()] = name;
            return {};
        }

        expected<void> stream_put(Node<Call> call) {
            auto args = call.ref(a)->arguments.ref(a);
            if (!args || args->arguments.size() != 1) {
                return unexpect_loc_error(call, "output.put with other than one argument");
            }
            auto v = args->arguments[0].ref(a)->value;
            MAYBE(text, p.expr(v));
            auto t = strip_wrappers(a, v.ref(a)->type);
            if (auto st = t.as_any<StructType>(); st && st.ref(a)->base.as_any<Format>()) {
                call_nested(text, format_of(a, t));
                return {};
            }
            auto have = p.type_of(v);
            if (known(have) && have != "uint8" && have != "int8") {
                return unexpect_loc_error(call, "output.put of {} (only bytes and formats)", have);
            }
            w.writeln("out = append(out, byte(", text, "))");
            return {};
        }

        expected<void> append_element(Node<Expr> base, Node<Expr> value) {
            MAYBE(b, p.expr(base));
            MAYBE(v, p.expr(value));
            auto elem = p.type_of(base);
            if (auto pos = elem.find(']'); pos != std::string::npos) {
                elem = elem.substr(pos + 1);
            }
            w.writeln(b, " = append(", b, ", ", convert(v, p.type_of(value), elem), ")");
            return {};
        }

        expected<void> loop_begin(Node<Expr> cond) {
            if (cond) {
                MAYBE(c, p.expr(cond));
                w.writeln("for ", c, " {");
            }
            else {
                w.writeln("for {");
            }
            ind.enter();
            return {};
        }

        expected<void> range_begin(Node<VariableDefinition> binding) {
            auto r = binding.ref(a);
            auto name = local_name(ident_text(a, r->name));
            MAYBE(n, p.expr(r->value));
            auto t = p.type_of(r->value);
            if (!known(t) || !is_go_int(t)) {
                t = "uint64";
            }
            p.locals[name] = t;
            w.writeln("for ", name, " := ", t, "(0); ", name, " < ", convert(n, p.type_of(r->value), t), "; ", name, "++ {");
            ind.enter();
            return {};
        }

        expected<void> loop_end() {
            ind.leave();
            w.writeln("}");
            return {};
        }

        expected<void> flow(Node<Statement> s) {
            if (s.as_any<Break>()) {
                w.writeln("break");
            }
            else if (s.as_any<Continue>()) {
                w.writeln("continue");
            }
            else {
                if (s.as_any<Return>().ref(a)->expr) {
                    return unexpect_loc_error(s, "return with a value in encode / decode");
                }
                w.writeln(mode == Mode::append ? "return out, nil" : "return nil");
            }
            return {};
        }

        // ---- Walker の既定のある口: Go が native に書けるものだけ上書き ----

        static bool native_width(std::size_t bytes) {
            return bytes == 1 || bytes == 2 || bytes == 4 || bytes == 8;
        }

        // 1/2/4/8 バイトの値を読む式。src はそのバイトの先頭から始まるスライス。
        static std::string read_uint(std::string_view order, std::size_t bytes, std::string_view src) {
            if (bytes == 1) {
                return std::format("{}[0]", src);
            }
            return std::format("{}.Uint{}({})", order, bytes * 8, src);
        }

        static std::string order_of(Endian e, bool native) {
            if (native) {
                return "binary.NativeEndian";
            }
            return e == Endian::little ? "binary.LittleEndian" : "binary.BigEndian";
        }

        // decode: bytes バイトを読み、それを指すスライスの綴りを返す。
        std::string take(NodeAny origin, std::size_t bytes) {
            auto n = std::to_string(bytes);
            if (mode == Mode::slice) {
                auto name = derived_name("in", origin);
                need(n);
                w.writeln(name, " := buf[*off : *off+", n, "]");
                w.writeln("*off += ", n);
                return name;
            }
            auto name = derived_name("in", origin);
            w.writeln("var ", name, " [", n, "]byte");
            read_full(name + "[:]");
            return name + "[:]";
        }

        // encode: 1/2/4/8 バイトの値 v を out に足す。
        void put_uint(std::string_view order, std::size_t bytes, std::string_view v) {
            if (bytes == 1) {
                w.writeln("out = append(out, byte(", v, "))");
                return;
            }
            w.writeln("out = ", order, ".AppendUint", std::to_string(bytes * 8), "(out, uint", std::to_string(bytes * 8),
                      "(", v, "))");
        }

        expected<void> check_equal(Node<Field> f, Node<Expr> target, Node<Expr> ex) {
            auto b = builder(f);
            return p.stmt(w, b.assert_(b.bin(BinaryOp::equal, target, ex, b.bool_type())));
        }

        expected<void> scalar(Node<Field> f, const lowering::ScalarShape& s, Node<Expr> target) {
            bool order_known = s.order != Endian::unspec || s.native_order;
            if (!native_width(s.bytes) || !order_known) {
                return Walker::scalar(f, s, target);  // lowering の文の形
            }
            auto order = order_of(s.order, s.native_order);
            auto bits = std::to_string(s.bytes * 8);
            auto ex = field_args(a, f).expected;
            if (decode) {
                auto src = take(f, s.bytes);
                auto raw = read_uint(order, s.bytes, src);
                std::string value;
                std::string have;
                switch (s.kind) {
                    case lowering::ScalarShape::Kind::float_:
                        value = std::format("math.Float{}frombits({})", bits, raw);
                        have = "float" + bits;
                        break;
                    case lowering::ScalarShape::Kind::enum_: {
                        MAYBE(en, p.type_name(s.type));
                        value = std::format("{}({})", en, raw);
                        have = en;
                        break;
                    }
                    case lowering::ScalarShape::Kind::int_:
                        have = (s.is_signed ? "int" : "uint") + bits;
                        value = s.is_signed ? std::format("{}({})", have, raw) : raw;
                        break;
                }
                MAYBE(t, p.expr(target));
                w.writeln(t, " = ", convert(value, have, p.type_of(target)));
                if (ex) {
                    MAYBE_VOID(r, check_equal(f, target, ex));
                }
                return {};
            }
            Node<Expr> v = target ? target : ex;
            std::string text = "0";
            if (v) {
                MAYBE(t, p.expr(v));
                text = t;
            }
            if (target && ex) {
                MAYBE_VOID(r, check_equal(f, target, ex));
            }
            if (s.kind == lowering::ScalarShape::Kind::float_) {
                text = std::format("math.Float{}bits({})", bits, text);
            }
            put_uint(order, s.bytes, text);
            return {};
        }

        // 平らなビット群: 群を 1 つの整数として読み、shift と mask で切り出す。
        expected<void> flat_group(const lowering::BitGroup& g) {
            const char* why = nullptr;
            auto shape = lowering::bit_group_shape(lc, g, &why);
            if (!shape) {
                return unexpect_loc_error(g.fields.front(), "bit group: {}", why ? why : "refused");
            }
            if (!native_width(shape->bytes)) {
                return Walker::flat_group(g);  // lowering の文の形
            }
            auto first = g.fields.front();
            auto order = order_of(shape->byte_order, false);
            auto uint = "uint" + std::to_string(shape->bytes * 8);
            auto v = derived_name("bits", first);
            if (decode) {
                auto src = take(first, shape->bytes);
                w.writeln(v, " := ", uint, "(", read_uint(order, shape->bytes, src), ")");
                for (auto& m : shape->members) {
                    auto part = std::format("({} >> {}) & {}", v, m.shift, m.mask);
                    if (name_of(a, m.field).empty()) {
                        if (m.expected) {
                            MAYBE(e, p.expr(m.expected));
                            block(std::format("if {} != {}", part, convert(e, p.type_of(m.expected), uint)),
                                  p.fail(assert_error((unparse_node(a, m.expected)))));
                        }
                        continue;
                    }
                    auto target = lowering::field_ref(lc, m.field);
                    MAYBE(tn, p.type_name(m.type));
                    MAYBE(t, p.expr(target));
                    w.writeln(t, " = ", convert(std::format("{}({})", tn, part), tn, p.type_of(target)));
                    if (m.expected) {
                        MAYBE_VOID(r, check_equal(m.field, target, m.expected));
                    }
                }
                return {};
            }
            w.writeln(v, " := ", uint, "(0)");
            for (auto& m : shape->members) {
                Node<Expr> value;
                if (!name_of(a, m.field).empty()) {
                    value = lowering::field_ref(lc, m.field);
                    if (m.expected) {
                        MAYBE_VOID(r, check_equal(m.field, value, m.expected));
                    }
                }
                else {
                    value = m.expected;  // 無ければ 0 のまま
                }
                if (!value) {
                    continue;
                }
                MAYBE(text, p.expr(value));
                w.writeln(v, " |= (", uint, "(", text, ") & ", std::to_string(m.mask), ") << ", std::to_string(m.shift));
            }
            put_uint(order, shape->bytes, v);
            return {};
        }

        // []byte はスライスのまま切り出す / 足す。それ以外は lowering の文の形。
        expected<void> array(Node<Field> f, Node<ArrayType> arr) {
            auto it = strip_wrappers(a, arr.ref(a)->element_type).as_any<IntType>();
            auto len = arr.ref(a)->length;
            if (len && !len.as_any<Range>() && !const_uint(a, tables, len)) {
                if (auto lt = p.type_of(len); known(lt) && !is_go_int(lt)) {
                    return unexpect_loc_error(f, "array length is not an integer ({})", lt);
                }
            }
            auto args = field_args(a, f);
            bool bytes = it && it.ref(a)->bit_size == 8 && !it.ref(a)->is_signed;
            if (!bytes || args.positional || named_args(f) || !len || len.as_any<Range>() || name_of(a, f).empty()) {
                return Walker::array(f, arr);
            }
            auto target = lowering::field_ref(lc, f);
            MAYBE(t, p.expr(target));
            if (auto key = lowering::xor_key(lc, f)) {
                // `input.xor = key`: 読んだバイト列を key で XOR する。読んだ元を
                // 書き換えないよう、スライスの口でも別の領域に入れる。
                MAYBE(k, p.expr(key));
                auto fixed = const_uint(a, tables, len);
                if (decode) {
                    MAYBE(raw, acquire(f, fixed ? Node<Expr>{} : len, fixed ? *fixed : 0));
                    MAYBE(rt, p.expr(raw));
                    if (!fixed) {
                        w.writeln(t, " = make([]byte, len(", rt, "))");
                    }
                    xor_into(t + "[:]", rt + "[:]", k + "[:]");
                    return {};
                }
                if (!fixed) {
                    MAYBE_VOID(r, check_count(target, len));
                }
                auto start = derived_name("start", f);
                w.writeln(start, " := len(out)");
                w.writeln("out = append(out, ", t, "[:]...)");
                xor_into("out[" + start + ":]", "out[" + start + ":]", k + "[:]");
                return {};
            }
            if (auto n = const_uint(a, tables, len); n && !p.type_of(target).starts_with("[]")) {
                auto count = std::to_string(*n);
                switch (mode) {
                    case Mode::slice:
                        need(count);
                        w.writeln("copy(", t, "[:], buf[*off:*off+", count, "])");
                        w.writeln("*off += ", count);
                        break;
                    case Mode::reader:
                        read_full(t + "[:]");
                        break;
                    case Mode::append:
                        w.writeln("out = append(out, ", t, "[:]...)");
                        break;
                }
                return {};
            }
            MAYBE(n, p.expr(len));
            auto nv = derived_name("n", f);
            switch (mode) {
                case Mode::slice:
                    w.writeln(nv, " := int(", n, ")");
                    block(std::format("if {} < 0 || len(buf)-*off < {}", nv, nv), "return " + short_err());
                    w.writeln(t, " = buf[*off : *off+", nv, "]");
                    w.writeln("*off += ", nv);
                    break;
                case Mode::reader:
                    w.writeln("{");
                    {
                        auto s = w.indent_scope();
                        read_n("b", "int(" + n + ")");
                        w.writeln(t, " = b");
                    }
                    w.writeln("}");
                    break;
                case Mode::append:
                    MAYBE_VOID(r, check_count(target, len));
                    w.writeln("out = append(out, ", t, "...)");
                    break;
            }
            return {};
        }
    };

    // struct のメンバを並び順に集める。分岐の腕の中の同名 field は、整数
    // どうしなら一番広い型に寄せる。
    expected<void> collect_members(GoPrinter& g, Node<Body> b, std::vector<std::string>& order) {
        auto& a = g.a;
        for (auto& s : b.ref(a)->statements) {
            if (auto f = s.as_any<Field>()) {
                if (!is_layout_field(a, f) || strip_wrappers(a, f.ref(a)->type).as_any<StructUnionType>()) {
                    continue;
                }
                auto name = std::string(name_of(a, f));
                if (name.empty()) {
                    continue;
                }
                MAYBE(t, g.type_name(f.ref(a)->type));
                auto [it, fresh] = g.members.try_emplace(name, t);
                if (fresh) {
                    order.push_back(name);
                }
                else if (it->second != t) {
                    // 腕ごとに長さの違う固定長配列 (`[16]u8` / `[4]u8`) はスライスにする。
                    auto elem_of = [](std::string_view ty) -> std::string_view {
                        auto p = ty.find(']');
                        return ty.starts_with("[") && p != std::string_view::npos ? ty.substr(p + 1) : std::string_view{};
                    };
                    if (!elem_of(it->second).empty() && elem_of(it->second) == elem_of(t)) {
                        it->second = "[]" + std::string(elem_of(t));
                        continue;
                    }
                    if (!is_go_int(it->second) || !is_go_int(t)) {
                        return unexpect_loc_error(f, "field {} has different types in different branches", name);
                    }
                    it->second = wider(it->second, t);
                }
                continue;
            }
            if (auto c = s.as_any<ConditionalExpr>()) {
                for (auto& blk : c.ref(a)->blocks) {
                    MAYBE_VOID(r, collect_members(g, blk.ref(a)->body, order));
                }
            }
        }
        return {};
    }

    // 生成した宣言と、それが型として使う名前 (値で持つものと全部)。
    struct Emitted {
        CodeWriter w;
        std::set<std::string> uses;
        std::set<std::string> value_uses;  // スライス越しでないもの (再帰すると Go の型にならない)
    };

    void names_in(std::string_view type, std::set<std::string>& out) {
        std::string cur;
        for (std::size_t i = 0; i <= type.size(); i++) {
            char ch = i < type.size() ? type[i] : ' ';
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
                cur += ch;
                continue;
            }
            if (!cur.empty() && std::isupper(static_cast<unsigned char>(cur[0]))) {
                out.insert(cur);
            }
            cur.clear();
        }
    }

    expected<Emitted> emit_format(Arena& a, SideTables& tables, Node<Format> fmt) {
        GoPrinter p{.a = a, .tables = tables, .lc = lowering::Context{a, tables}};
        auto* state = tables.table<FormatState>().get(fmt);
        Node<Function> custom_decode = state && state->decode_kind == FormatKind::custom ? state->decode_custom : nullref;
        Node<Function> custom_encode = state && state->encode_kind == FormatKind::custom ? state->encode_custom : nullref;
        if ((state && state->decode_kind == FormatKind::custom && !custom_decode) ||
            (state && state->encode_kind == FormatKind::custom && !custom_encode)) {
            return unexpect_loc_error(fmt, "custom encode / decode without its function");
        }
        auto name = camel(name_of(a, fmt));
        std::vector<std::string> order;
        MAYBE_VOID(m, collect_members(p, fmt.ref(a)->body, order));

        // 口ごとに本体を展開する。
        auto expand = [&](Mode mode, bool decode) -> expected<CodeWriter> {
            CodeWriter w;
            p.locals.clear();
            p.mode = mode;
            p.replaced.clear();
            GoCodec g{p, w, mode};
            auto custom = decode ? custom_decode : custom_encode;
            if (custom) {
                MAYBE_VOID(r, g.run_custom(custom, decode));
            }
            else {
                MAYBE_VOID(r, g.run(fmt, decode));
            }
            return w;
        };
        MAYBE(slice, expand(Mode::slice, true));
        MAYBE(reader, expand(Mode::reader, true));
        MAYBE(append, expand(Mode::append, false));

        CodeWriter w;
        w.writeln("type ", name, " struct {");
        {
            auto s = w.indent_scope();
            for (auto& n : order) {
                w.writeln(camel(n), " ", p.members[n]);
            }
        }
        w.writeln("}");
        auto func = [&](std::string_view sig, CodeWriter&& b, std::initializer_list<std::string_view> head,
                        std::string_view tail) {
            w.writeln();
            w.writeln("func (t *", name, ") ", sig, " {");
            {
                auto s = w.indent_scope();
                for (auto h : head) {
                    w.writeln(h);
                }
                w.write(std::move(b));
                w.writeln(tail);
            }
            w.writeln("}");
        };
        // state を受け取る format は本体を *Impl にして state を引数で受け、公開の
        // 口は新しい state を作って呼ぶ (ebm2go の *_impl と同じ形)。
        // 本体の最後が return / 抜け出しの無いループなら、末尾の return は届かない (vet が咎める)。
        bool dec_terminal = ends_terminal(a, custom_decode ? custom_decode.ref(a)->body : fmt.ref(a)->body);
        bool enc_terminal = ends_terminal(a, custom_encode ? custom_encode.ref(a)->body : fmt.ref(a)->body);
        std::string_view dec_tail = dec_terminal ? "" : "return nil";
        std::string_view enc_tail = enc_terminal ? "" : "return out, nil";
        auto dec_states = state_params(tables, fmt, true);
        auto enc_states = state_params(tables, fmt, false);
        auto sig = [&](const std::vector<Node<StateVariable>>& v) {
            std::string out;
            for (auto sv : v) {
                auto t = p.type_name(sv.ref(a)->type);
                out += std::format(", {} *{}", state_name(a, sv), t ? *t : "struct{}");
            }
            return out;
        };
        auto fresh = [&](const std::vector<Node<StateVariable>>& v) {
            std::vector<std::string> out;
            for (auto sv : v) {
                auto t = p.type_name(sv.ref(a)->type);
                out.push_back(std::format("{} := &{}{{}}", state_name(a, sv), t ? *t : "struct{}"));
            }
            return out;
        };
        auto wrapper = [&](std::string_view public_sig, const std::vector<Node<StateVariable>>& v, std::string call) {
            w.writeln();
            w.writeln("func (t *", name, ") ", public_sig, " {");
            {
                auto s = w.indent_scope();
                for (auto& line : fresh(v)) {
                    w.writeln(line);
                }
                w.writeln("return ", call);
            }
            w.writeln("}");
        };
        if (dec_states.empty()) {
            func("DecodeSlice(buf []byte, off *int) error", std::move(slice), {}, dec_tail);
            func("Read(r io.Reader) error", std::move(reader), {}, dec_tail);
        }
        else {
            auto args = state_args(a, dec_states);
            func(std::format("decodeSliceImpl(buf []byte, off *int{}) error", sig(dec_states)), std::move(slice), {}, dec_tail);
            func(std::format("readImpl(r io.Reader{}) error", sig(dec_states)), std::move(reader), {}, dec_tail);
            wrapper("DecodeSlice(buf []byte, off *int) error", dec_states, std::format("t.decodeSliceImpl(buf, off{})", args));
            wrapper("Read(r io.Reader) error", dec_states, std::format("t.readImpl(r{})", args));
        }
        if (enc_states.empty()) {
            func("Append(out []byte) ([]byte, error)", std::move(append), {"var err error", "_ = err"}, enc_tail);
        }
        else {
            func(std::format("appendImpl(out []byte{}) ([]byte, error)", sig(enc_states)), std::move(append),
                 {"var err error", "_ = err"}, enc_tail);
            wrapper("Append(out []byte) ([]byte, error)", enc_states,
                    std::format("t.appendImpl(out{})", state_args(a, enc_states)));
        }
        CodeWriter none;
        func("Decode(buf []byte) ([]byte, error)", std::move(none),
             {"var off int", "if err := t.DecodeSlice(buf, &off); err != nil {", "\treturn nil, err", "}"},
             "return buf[off:], nil");
        CodeWriter none2;
        func("Encode(buf []byte) ([]byte, error)", std::move(none2), {}, "return t.Append(buf[:0])");
        CodeWriter none3;
        func("Write(w io.Writer) error", std::move(none3),
             {"b, err := t.Append(nil)", "if err != nil {", "\treturn err", "}", "_, err = w.Write(b)"}, "return err");

        Emitted out{.w = std::move(w)};
        for (auto* v : {&dec_states, &enc_states}) {
            for (auto sv : *v) {
                if (auto t = p.type_name(sv.ref(a)->type)) {
                    names_in(*t, out.uses);
                }
            }
        }
        for (auto& [_, t] : p.members) {
            names_in(t, out.uses);
            if (!t.starts_with("[]")) {
                names_in(t, out.value_uses);
            }
        }
        return out;
    }

    expected<CodeWriter> emit_enum(Arena& a, SideTables& tables, Node<Enum> en) {
        GoPrinter p{.a = a, .tables = tables, .lc = lowering::Context{a, tables}};
        auto r = en.ref(a);
        // 下地の型の無い enum は読み書きされない (field の型にならない)。値を
        // 区別できればよいので Go の int にする。
        std::string base = "int";
        if (r->base_type) {
            MAYBE(b, p.type_name(r->base_type));
            base = b;
        }
        auto name = camel(name_of(a, en));
        CodeWriter w;
        w.writeln("type ", name, " ", base);
        w.writeln();
        w.writeln("const (");
        {
            auto s = w.indent_scope();
            for (auto& m : r->members) {
                auto v = const_uint(a, tables, m.ref(a)->value);
                if (!v) {
                    return unexpect_loc_error(m, "enum member whose value is not a constant");
                }
                if (auto bits = int_bits_of(base); bits > 0 && bits < 64 && *v >> bits) {
                    return unexpect_loc_error(m, "enum member value {} does not fit in {}", *v, base);
                }
                w.writeln(name, camel(name_of(a, m)), " ", name, " = ", std::to_string(*v));
            }
        }
        w.writeln(")");
        return w;
    }

    // pkg の公開名 (`pkg.Upper`) を使っているか。使っていない import は Go のコンパイル
    // エラーなので、本体を組んでから import を決める。
    bool uses_pkg(std::string_view text, std::string_view pkg) {
        for (std::size_t pos = text.find(pkg); pos != std::string_view::npos; pos = text.find(pkg, pos + 1)) {
            auto end = pos + pkg.size();
            bool head = pos == 0 || !(std::isalnum(static_cast<unsigned char>(text[pos - 1])) || text[pos - 1] == '_' || text[pos - 1] == '.');
            bool tail = end + 1 < text.size() && text[end] == '.' && std::isupper(static_cast<unsigned char>(text[end + 1]));
            if (head && tail) {
                return true;
            }
        }
        return false;
    }

    void report(std::string_view msg) {
        futils::wrap::cerr_wrap() << "nast2go: " << msg << '\n';
    }

}  // namespace

NAST_BACKEND_ENTRY(GoConfig) {
    knobs.bind_Module(ctx, [](LangContext& c, Node<Module> n) -> expected<CodeWriter> {
        auto& a = c.arena();
        auto& tables = c.tables();
        // 入れ子の format / enum も外へ出す (Go に入れ子の型宣言は無い)。
        // 全部組んでから、飛ばした宣言を使うものと、値で自分を含むものを落とす。
        struct Decl {
            std::string name;
            std::optional<Emitted> out;
            std::string skipped;  // 飛ばした理由
        };
        std::vector<Decl> decls;
        std::set<std::string> seen;
        std::vector<std::string> skipped;  // 外した宣言 (--unhandled error で失敗にする)
        auto skip = [&](std::string msg) {
            report(msg);
            skipped.push_back(std::move(msg));
        };
        auto each_decl = [&](auto&& self, Node<Body> b) -> void {
            for (auto& s : b.ref(a)->statements) {
                std::string name(name_of(a, s));
                auto f = s.as_any<Format>();
                auto en = s.as_any<Enum>();
                if (auto st = s.as_any<State>()) {
                    // `state X:` は読み書きしない入れ物。struct だけ出す。
                    if (!seen.insert(camel(name)).second) {
                        continue;
                    }
                    Decl d{.name = camel(name)};
                    GoPrinter sp{.a = a, .tables = tables, .lc = lowering::Context{a, tables}};
                    std::vector<std::string> order;
                    if (auto r = collect_members(sp, st.ref(a)->body, order); !r) {
                        d.skipped = r.error().message;
                    }
                    else {
                        Emitted e;
                        e.w.writeln("type ", camel(name), " struct {");
                        {
                            auto ind = e.w.indent_scope();
                            for (auto& m : order) {
                                e.w.writeln(camel(m), " ", sp.members[m]);
                            }
                        }
                        e.w.writeln("}");
                        for (auto& [_, t] : sp.members) {
                            names_in(t, e.uses);
                            if (!t.starts_with("[]")) {
                                names_in(t, e.value_uses);
                            }
                        }
                        d.out = std::move(e);
                    }
                    decls.push_back(std::move(d));
                    continue;
                }
                if (!f && !en) {
                    continue;
                }
                if (!seen.insert(camel(name)).second) {
                    continue;
                }
                Decl d{.name = camel(name)};
                if (f) {
                    auto r = emit_format(a, tables, f);
                    if (r) {
                        d.out = std::move(*r);
                    }
                    else {
                        auto loc = r.error().node.ref(a).loc();
                        d.skipped = std::format("{}:{}: {}", loc.line + 1, loc.col + 1, r.error().message);
                    }
                }
                else {
                    auto r = emit_enum(a, tables, en);
                    if (r) {
                        d.out = Emitted{.w = std::move(*r)};
                    }
                    else {
                        d.skipped = r.error().message;
                    }
                }
                decls.push_back(std::move(d));
                if (f) {
                    self(self, f.ref(a)->body);
                }
            }
        };
        // import した module の宣言も同じファイルに出す (Go のパッケージは 1 つ)。
        std::vector<Node<Module>> modules{n};
        each_node<Import>(a, [&](Node<Import> imp) {
            if (auto* res = tables.table<ImportResolution>().get(imp); res && res->module) {
                if (std::find(modules.begin(), modules.end(), res->module) == modules.end()) {
                    modules.push_back(res->module);
                }
            }
        });
        for (auto m : modules) {
            auto top = a.make<Body>(m.ref(a).loc());
            top->statements = m.ref(a)->statements;
            each_decl(each_decl, top);
        }

        std::map<std::string, Decl*> by_name;
        for (auto& d : decls) {
            by_name[d.name] = &d;
        }
        // 値で自分へ戻ってくるもの (A が B を、B が A を値で持つ)。
        for (auto& d : decls) {
            if (!d.out) {
                continue;
            }
            std::set<std::string> visited;
            std::vector<std::string> stack(d.out->value_uses.begin(), d.out->value_uses.end());
            while (!stack.empty()) {
                auto cur = stack.back();
                stack.pop_back();
                if (cur == d.name) {
                    d.skipped = "contains itself by value (not a valid Go type)";
                    break;
                }
                if (!visited.insert(cur).second) {
                    continue;
                }
                if (auto it = by_name.find(cur); it != by_name.end() && it->second->out) {
                    stack.insert(stack.end(), it->second->out->value_uses.begin(), it->second->out->value_uses.end());
                }
            }
            if (!d.skipped.empty()) {
                d.out.reset();
            }
        }
        // 飛ばした宣言を使うものを落とす。落とすと次が出るので止まるまで。
        for (bool changed = true; changed;) {
            changed = false;
            for (auto& d : decls) {
                if (!d.out) {
                    continue;
                }
                for (auto& u : d.out->uses) {
                    auto it = by_name.find(u);
                    if (it == by_name.end() || !it->second->out) {
                        d.skipped = it == by_name.end() ? std::format("uses undeclared type {}", u)
                                                        : std::format("uses skipped {}", u);
                        d.out.reset();
                        changed = true;
                        break;
                    }
                }
            }
        }
        CodeWriter body;
        // トップレベルの定数 (`MAGIC ::= 0x...`)。整数に畳めたものは型を付けない
        // Go の定数にして、使う側の型に合わせさせる。
        std::set<std::string> seen_consts;
        std::vector<Node<Statement>> top_statements;
        for (auto m : modules) {
            for (auto& s : m.ref(a)->statements) {
                top_statements.push_back(s);
            }
        }
        for (auto& s : top_statements) {
            auto def = s.as_any<VariableDefinition>();
            if (!def || strip_paren(a, def.ref(a)->value).as_any<Import>()) {
                continue;  // `q ::= import(...)` は名前空間で、値ではない
            }
            if (!seen_consts.insert(local_name(name_of(a, def))).second) {
                continue;
            }
            auto name = local_name(name_of(a, def));
            if (auto v = const_uint(a, tables, def.ref(a)->value)) {
                body.writeln();
                body.writeln("const ", name, " = ", std::to_string(*v));
                continue;
            }
            // `TypeA ::= u16(1)` のような型つきの定数。定数どうしの参照が自分へ
            // 戻るものは Go で初期化できない。
            GoPrinter p{.a = a, .tables = tables, .lc = lowering::Context{a, tables}};
            auto text = p.expr(def.ref(a)->value);
            bool cyclic = false;
            std::set<std::uint32_t> visited;
            std::vector<NodeAny> stack{def.ref(a)->value};
            while (!stack.empty() && !cyclic) {
                auto cur = stack.back();
                stack.pop_back();
                visit_all(a, cur, [&](NodeAny x) {
                    if (auto r = x.as_any<Reference>()) {
                        auto* res = tables.table<Resolution>().get(r.ref(a)->name);
                        if (auto d2 = res ? res->target.as_any<VariableDefinition>() : Node<VariableDefinition>{}) {
                            if (d2 == def) {
                                cyclic = true;
                            }
                            else if (visited.insert(d2.id()).second) {
                                stack.push_back(d2.ref(a)->value);
                            }
                        }
                    }
                    return !cyclic;
                });
            }
            if (cyclic) {
                skip(std::format("skipped constant {}: refers to itself", name));
            }
            else if (!text) {
                skip(std::format("skipped constant {}: {}", name, text.error().message));
            }
            else {
                // 三項 (即時の関数) は Go の定数にならないので var にする。
                body.writeln();
                body.writeln(text->find("func()") == std::string::npos ? "const " : "var ", name, " = ", *text);
            }
        }
        for (auto& d : decls) {
            body.writeln();
            if (d.out) {
                body.write(std::move(d.out->w));
                continue;
            }
            auto msg = std::format("skipped {}: {}", d.name, d.skipped);
            skip(msg);
            if (c.config().unhandled_mode == UnhandledMode::dummy) {
                body.writeln("// nast2go: ", msg);
            }
        }
        // 書けなかった宣言は丸ごと外す (書きかけを 1 つ混ぜると Go はパッケージ
        // ごとコンパイルできなくなり、書けた宣言まで使えなくなる)。外したことの
        // 扱いは --unhandled に従う: dummy は出力に理由を残す、ignore は残さない、
        // error は失敗にする。理由はどれでも stderr に出す。
        if (c.config().unhandled_mode == UnhandledMode::error && !skipped.empty()) {
            return unexpect_loc_error(n, "{} declaration(s) could not be generated; the first: {}", skipped.size(), skipped.front());
        }
        // パッケージ直下には型と定数しか出さない。routine を置くと、生成した
        // ファイルを 2 つ同じパッケージに置いた時点で衝突する (ebm2go と同じ性質)。
        // import は使った分だけ。
        auto text = body.to_string("\t");
        CodeWriter w;
        w.writeln("// Code generated by nast2go. DO NOT EDIT.");
        w.writeln("package ", c.lang_config().package);
        std::vector<std::string_view> imports;
        for (auto [pkg, path] : {std::pair<std::string_view, std::string_view>{"bytes", "bytes"},
                                 {"binary", "encoding/binary"},
                                 {"errors", "errors"},
                                 {"fmt", "fmt"},
                                 {"io", "io"},
                                 {"math", "math"}}) {
            if (uses_pkg(text, pkg)) {
                imports.push_back(path);
            }
        }
        if (!imports.empty()) {
            w.writeln();
            w.writeln("import (");
            {
                auto s = w.indent_scope();
                for (auto path : imports) {
                    w.writeln("\"", path, "\"");
                }
            }
            w.writeln(")");
        }
        w.write(std::move(body));
        return w;
    });
    return {};
}
