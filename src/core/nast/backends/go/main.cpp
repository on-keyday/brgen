/*license*/
// Go のバックエンド (2 代目)。
//
//   ./nast2go -i input.bgn            標準出力へ
//   ./nast2go input1.bgn input2.bgn   まとめて (--survey と併用)
//
// 初代 (commit 7cc9c66f で削除) は AST から直に codec を書いて、ebmgen の
// transform の段を言語の中で導出し直すことになった
// (docs/size_and_lowering.md §1)。こちらは codec の形を lowering/ に組ませ、
// 返ってきた nast のノードを Go に綴るだけにする:
//
//   field の並びとビット群   lowering::bit_layout
//   整数 / enum / 配列       lowering::lower_field_decode / encode
//   ビット群                 lowering::lower_bit_group_* / lower_routed_bit_*
//   バイトの出し入れ         lowering::read_bytes / write_bytes (input.get() / output.put(x))
//   match                    lowering::lower_match (if の連鎖)
//   範囲との比較             lowering::lower_range_compare
//
// ここに残るのは lowering が呼ぶ側に任せたもの: バッファの宣言、読み終えた
// ことの検査、入れ子 format の呼び出し、配列の確保、Go の型の付け方。
//
// Go の実行時は decoder / encoder の 2 つだけで、どちらも 1 バイトずつ
// (`input.get()` / `output.put(x)` に当たる)。読み損ねは decoder が抱えて
// おき、読んだ後に 1 度確かめる。
//
// 組めない format は飛ばし、理由を出力のコメントと標準エラーに出す。
#include "../../backend/entry.hpp"
#include "../../lowering/bit_group.hpp"
#include "../../lowering/field_io.hpp"
#include "../../lowering/match_to_if.hpp"
#include "../../lowering/predicate.hpp"
#include "../../lowering/self_ref.hpp"
#include "../../lowering/stream_io.hpp"
#include "../../node/build.h"
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

    std::string exported(std::string_view name) {
        std::string out(name);
        if (!out.empty()) {
            out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
        }
        return out;
    }

    // 局所変数の名前。Go の予約語と、よく使う組み込みの名前を避ける。
    std::string local_name(std::string_view name) {
        static const std::set<std::string_view> reserved = {
            "break", "case", "chan", "const", "continue", "default", "defer", "else", "fallthrough", "for",
            "func", "go", "goto", "if", "import", "interface", "map", "package", "range", "return",
            "select", "struct", "switch", "type", "var", "len", "cap", "make", "new", "append", "copy",
            "string", "byte", "int", "uint", "error", "true", "false", "nil", "t", "d", "e", "err"};
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

    // --- 型 ----------------------------------------------------------------

    struct Go;

    std::size_t int_bits_of(std::string_view go) {
        auto digits = go.substr(go.find_first_of("0123456789") == std::string_view::npos ? go.size() : go.find_first_of("0123456789"));
        std::size_t n = 0;
        for (auto c : digits) {
            n = n * 10 + (c - '0');
        }
        return n;
    }

    bool is_go_int(std::string_view go) {
        return go.starts_with("uint") || go.starts_with("int");
    }

    // 1 つの format を綴る間の状態。
    struct Go {
        Arena& a;
        SideTables& tables;
        lowering::Context lc;
        Node<Format> fmt;
        // struct のメンバ。名前から Go の型へ。
        std::map<std::string, std::string> members;
        // 局所変数。名前から Go の型へ (分かるものだけ)。
        std::map<std::string, std::string> locals;
        lowering::BitLayout layout;
        bool decode = true;
        std::set<std::size_t> routed_declared;

        expected<std::string> type_name(Node<Type> t);
        expected<std::string> expr(Node<Expr> e);
        std::string type_of(Node<Expr> e);
        expected<void> stmt(CodeWriter& w, Node<Statement> s);
        expected<void> body(CodeWriter& w, Node<Body> b);
        expected<void> codec(CodeWriter& w, Node<Body> b);
        expected<void> field(CodeWriter& w, Node<Field> f);
        expected<void> plain_field(CodeWriter& w, Node<Field> f);
        expected<void> flat_group(CodeWriter& w, const lowering::BitGroup& g);
        expected<void> routed_member(CodeWriter& w, std::size_t group, std::size_t member);
        expected<void> branch(CodeWriter& w, Node<ConditionalExpr> cond);
        Node<Expr> target_of(CodeWriter& w, Node<Field> f);
        void check_read(CodeWriter& w) {
            if (decode) {
                w.writeln("if d.err != nil {");
                {
                    auto s = w.indent_scope();
                    w.writeln("return d.err");
                }
                w.writeln("}");
            }
        }
        Builder builder(NodeAny origin) {
            return Builder{a, origin.ref(a).loc()};
        }
    };

    expected<std::string> Go::type_name(Node<Type> t) {
        if (!t) {
            return unexpect_loc_error(t, "型が無い");
        }
        t = strip_wrappers(a, t);
        if (auto i = t.as_any<IntType>()) {
            auto r = i.ref(a);
            auto bits = r->bit_size;
            if (bits == 0 || bits > 64) {
                return unexpect_loc_error(t, "{} ビットの整数は Go の型に無い", bits);
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
                return exported(name_of(a, f));
            }
        }
        if (auto e = t.as_any<EnumType>()) {
            return exported(name_of(a, e.ref(a)->base));
        }
        return unexpect_loc_error(t, "{} は Go の型にまだ写せない", to_string(t.type()));
    }

    // 式の Go の型。型の無いリテラルは空、分からなければ unknown_type。
    constexpr std::string_view unknown_type = "?";

    bool known(std::string_view t) {
        return !t.empty() && t != unknown_type;
    }

    std::string Go::type_of(Node<Expr> e) {
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
            auto n = type_name(t);
            if (n) {
                return *n;
            }
        }
        return std::string(unknown_type);
    }

    std::string wider(std::string_view l, std::string_view r) {
        if (!is_go_int(l) || !is_go_int(r)) {
            return std::string(l);
        }
        auto bits = std::max(int_bits_of(l), int_bits_of(r));
        bool sign = l.starts_with("int") || r.starts_with("int");
        return std::format("{}int{}", sign ? "" : "u", bits);
    }

    // value を want の型にして綴る。型が同じか、型の無いリテラルならそのまま。
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

    expected<std::string> Go::expr(Node<Expr> e) {
        if (!e) {
            return unexpect_loc_error(e, "式が無い");
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
            return std::string("t");
        }
        if (auto w = e.as_any<WithReceiver>()) {
            if (!strip_paren(a, w.ref(a)->receiver).as_any<Self>()) {
                return unexpect_loc_error(e, "self 以外のレシーバ");
            }
            return expr(w.ref(a)->expr);
        }
        if (auto r = e.as_any<Reference>()) {
            auto name = ident_text(a, r.ref(a)->name);
            auto* res = tables.table<Resolution>().get(r.ref(a)->name);
            if (!res) {
                return local_name(name);  // lowering が合成した名前
            }
            auto target = res->target;
            if (auto f = target.as_any<Field>()) {
                if (members.contains(std::string(name))) {
                    return "t." + exported(name);
                }
                return unexpect_loc_error(e, "この format のものでない field {}", name);
            }
            if (target.as_any<VariableDefinition>() || target.as_any<Parameter>()) {
                return local_name(name);
            }
            return unexpect_loc_error(e, "{} を指す名前", to_string(target.type()));
        }
        if (auto m = e.as_any<MemberAccess>()) {
            auto mr = m.ref(a);
            auto base = strip_paren(a, mr->base);
            auto member = ident_text(a, mr->member);
            if (auto br = base.as_any<Reference>()) {
                auto* res = tables.table<Resolution>().get(br.ref(a)->name);
                if (res && res->target.as_any<Enum>()) {
                    return exported(name_of(a, res->target)) + exported(member);
                }
            }
            if (base.as_any<SpecialLiteral>()) {
                return unexpect_loc_error(e, "input / output のメンバ {}", member);
            }
            MAYBE(b, expr(mr->base));
            if (member == "length" && type_of(mr->base).starts_with("[")) {
                auto t = type_of(e);
                return std::format("{}(len({}))", known(t) ? t : "uint64", b);
            }
            return b + "." + exported(member);
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
                return unexpect_loc_error(e, "引数が 1 つでない型変換");
            }
            auto value = args->arguments[0].ref(a)->value;
            MAYBE(to, type_name(c.ref(a)->type));
            MAYBE(v, expr(value));
            auto have = type_of(value);
            if (to == "bool" && is_go_int(have)) {
                return "(" + v + " != 0)";
            }
            if (have == "bool" && is_go_int(to)) {
                return "nastBoolToInt[" + to + "](" + v + ")";
            }
            return to + "(" + v + ")";
        }
        if (auto call = e.as_any<Call>()) {
            auto cr = call.ref(a);
            auto callee = strip_paren(a, cr->callee).as_any<MemberAccess>();
            if (callee) {
                auto base = strip_paren(a, callee.ref(a)->base).as_any<SpecialLiteral>();
                auto member = ident_text(a, callee.ref(a)->member);
                auto nargs = cr->arguments ? cr->arguments.ref(a)->arguments.size() : 0;
                if (base && base.ref(a)->kind == SpecialLiteralKind::input_ && member == "get" && nargs == 0) {
                    return std::string("d.get()");
                }
                if (base && base.ref(a)->kind == SpecialLiteralKind::output_ && member == "put" && nargs == 1) {
                    auto v = cr->arguments.ref(a)->arguments[0].ref(a)->value;
                    MAYBE(x, expr(v));
                    return "e.put(" + convert(x, type_of(v), "uint8") + ")";
                }
            }
            return unexpect_loc_error(e, "呼び出し");
        }
        if (auto ix = e.as_any<Index>()) {
            MAYBE(b, expr(ix.ref(a)->base));
            MAYBE(i, expr(ix.ref(a)->index));
            return b + "[" + i + "]";
        }
        if (auto c = e.as_any<Cond>()) {
            // 三項は即時の関数で包む。Go に式の分岐が無い。
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
        return unexpect_loc_error(e, "{} はまだ Go の式にならない", to_string(e.type()));
    }

    // --- 文 (lowering が組んだもの) ----------------------------------------

    expected<void> Go::body(CodeWriter& w, Node<Body> b) {
        if (!b) {
            return unexpect_loc_error(b, "Body が無い");
        }
        for (auto& s : b.ref(a)->statements) {
            MAYBE_VOID(r, stmt(w, s));
        }
        return {};
    }

    expected<void> Go::stmt(CodeWriter& w, Node<Statement> s) {
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
                w.writeln("return nastAssert(", go_string(unparse_node(a, cond)), ")");
            }
            w.writeln("}");
            return {};
        }
        if (auto loop = s.as_any<Loop>()) {
            auto r = loop.ref(a);
            auto init = r->init.as_any<VariableDefinition>();
            auto step = r->step.as_any<Assign>();
            if (!init || !step || !r->condition) {
                return unexpect_loc_error(s, "数えるループ以外のループ");
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
        if (auto cond = s.as_any<If>()) {
            return branch(w, cond);
        }
        if (auto call = s.as_any<Call>()) {
            MAYBE(text, expr(call));  // output.put(x)
            w.writeln(text);
            return {};
        }
        return unexpect_loc_error(s, "{} はまだ Go の文にならない", to_string(s.type()));
    }

    // --- codec -------------------------------------------------------------

    // field の書き先。名前の無い field は局所変数を作ってそこに読む。
    Node<Expr> Go::target_of(CodeWriter& w, Node<Field> f) {
        if (!name_of(a, f).empty()) {
            return lowering::field_ref(lc, f);
        }
        auto t = type_name(f.ref(a)->type);
        auto name = derived_name("pad", f);
        w.writeln("var ", name, " ", t ? *t : "uint8");
        w.writeln("_ = ", name);
        if (t) {
            locals[name] = *t;
        }
        return builder(f).ref(name, f.ref(a)->type);
    }

    expected<void> Go::flat_group(CodeWriter& w, const lowering::BitGroup& g) {
        auto b = builder(g.fields.front());
        auto n = g.bits / 8;
        auto buf_name = derived_name("buf", g.fields.front());
        w.writeln("var ", buf_name, " [", std::to_string(n), "]byte");
        locals[buf_name] = std::format("[{}]uint8", n);
        auto buf = b.ref(buf_name, nullref);
        std::vector<Node<Expr>> targets;
        for (auto f : g.fields) {
            if (!decode && name_of(a, f).empty()) {
                targets.push_back(nullref);  // 期待値か 0 を書く
                continue;
            }
            targets.push_back(target_of(w, f));
        }
        if (decode) {
            MAYBE_VOID(r, body(w, lowering::read_bytes(lc, lowering::input_stream(lc, b.loc), buf, nullref, b.lit(n))));
            check_read(w);
            auto lowered = lowering::lower_bit_group_decode(lc, g, targets, buf, nullref);
            if (!lowered) {
                return unexpect_loc_error(g.fields.front(), "ビット群が組めない");
            }
            return body(w, lowered);
        }
        auto lowered = lowering::lower_bit_group_encode(lc, g, targets, buf, nullref);
        if (!lowered) {
            return unexpect_loc_error(g.fields.front(), "ビット群が組めない");
        }
        MAYBE_VOID(r, body(w, lowered));
        return body(w, lowering::write_bytes(lc, lowering::output_stream(lc, b.loc), buf, nullref, b.lit(n)));
    }

    expected<void> Go::routed_member(CodeWriter& w, std::size_t group, std::size_t member) {
        auto& g = layout.routed[group];
        auto& f = g.fields[member];
        auto b = builder(f.field);
        auto buf_name = derived_name("buf", g.fields.front().field);
        if (!routed_declared.contains(group)) {
            routed_declared.insert(group);
            w.writeln("var ", buf_name, " [", std::to_string(g.max_bytes), "]byte");
            locals[buf_name] = std::format("[{}]uint8", g.max_bytes);
        }
        auto buf = b.ref(buf_name, nullref);
        Node<Expr> target = (!decode && name_of(a, f.field).empty()) ? Node<Expr>{} : target_of(w, f.field);
        if (decode) {
            if (f.fetch_to > f.fetch_from) {
                MAYBE_VOID(r, body(w, lowering::read_bytes(lc, lowering::input_stream(lc, b.loc), buf, b.lit(f.fetch_from),
                                                          b.lit(f.fetch_to - f.fetch_from))));
                check_read(w);
            }
            auto lowered = lowering::lower_routed_bit_decode(lc, g, member, target, buf, nullref);
            if (!lowered) {
                return unexpect_loc_error(f.field, "分岐を跨ぐビット群が組めない");
            }
            return body(w, lowered);
        }
        auto lowered = lowering::lower_routed_bit_encode(lc, g, member, target, buf, nullref);
        if (!lowered) {
            return unexpect_loc_error(f.field, "分岐を跨ぐビット群が組めない");
        }
        MAYBE_VOID(r, body(w, lowered));
        if (f.flush_to > f.flush_from) {
            return body(w, lowering::write_bytes(lc, lowering::output_stream(lc, b.loc), buf, b.lit(f.flush_from),
                                                 b.lit(f.flush_to - f.flush_from)));
        }
        return {};
    }

    // 入力から来た長さで確保する前に、残りの入力に収まるかを確かめる。
    // 長さの式は符号なしで割れて巨大になりうる (`length - 8` で length < 8)。
    void room(CodeWriter& w, std::string_view count, std::size_t width) {
        w.writeln("if !d.room(uint64(", count, "), ", std::to_string(width), ") {");
        {
            auto s = w.indent_scope();
            w.writeln("return io.ErrUnexpectedEOF");
        }
        w.writeln("}");
    }

    // 入れ子 format を 1 つ読む / 書く。
    void call_nested(Go& g, CodeWriter& w, std::string_view target) {
        w.writeln("if err := ", target, g.decode ? ".decode(d)" : ".encode(e)", "; err != nil {");
        {
            auto s = w.indent_scope();
            w.writeln("return err");
        }
        w.writeln("}");
    }

    expected<void> Go::plain_field(CodeWriter& w, Node<Field> f) {
        auto type = strip_wrappers(a, f.ref(a)->type);
        auto b = builder(f);
        if (auto s = type.as_any<StructType>(); s && s.ref(a)->base.as_any<Format>()) {
            if (f.ref(a)->arguments) {
                return unexpect_loc_error(f, "引数つきの入れ子 format");
            }
            MAYBE(t, expr(target_of(w, f)));
            call_nested(*this, w, t);
            return {};
        }
        auto target = target_of(w, f);
        MAYBE(target_text, expr(target));
        auto arr = type.as_any<ArrayType>();
        Node<Expr> count;  // 要素の数 (配列のとき)
        bool slice = false;
        if (arr) {
            auto len = arr.ref(a)->length;
            auto elem = strip_wrappers(a, arr.ref(a)->element_type);
            if (field_args(a, f).named) {
                return unexpect_loc_error(f, "名前つきの引数の付いた配列");
            }
            if (!len || len.as_any<Range>()) {
                // 末尾まで。バイト列だけ。
                auto it = elem.as_any<IntType>();
                if (!it || it.ref(a)->bit_size != 8 || field_args(a, f).positional) {
                    return unexpect_loc_error(f, "末尾までのバイト列以外の配列");
                }
                if (decode) {
                    w.writeln(target_text, " = d.rest()");
                }
                else {
                    w.writeln("e.out = append(e.out, ", target_text, "...)");
                }
                return {};
            }
            slice = !const_uint(a, tables, len);
            count = len;
            if (auto ct = type_of(len); slice && known(ct) && !is_go_int(ct)) {
                return unexpect_loc_error(f, "長さが整数でない ({})", ct);
            }
            if (auto s = elem.as_any<StructType>(); s && s.ref(a)->base.as_any<Format>()) {
                MAYBE(n, expr(len));
                MAYBE(elem_name, type_name(elem));
                if (slice && decode) {
                    room(w, n, 1);  // 要素は少なくとも 1 バイトとみなす
                    w.writeln(target_text, " = make([]", elem_name, ", int(", n, "))");
                }
                if (slice && !decode) {
                    w.writeln("if len(", target_text, ") != int(", n, ") {");
                    {
                        auto s2 = w.indent_scope();
                        w.writeln("return nastAssert(", go_string("len(" + std::string(name_of(a, f)) + ")"), ")");
                    }
                    w.writeln("}");
                }
                w.writeln("for i := range ", target_text, " {");
                {
                    auto s2 = w.indent_scope();
                    call_nested(*this, w, target_text + "[i]");
                }
                w.writeln("}");
                return {};
            }
        }
        // バイト幅の整数 / enum / 浮動小数点と、その固定幅の配列。
        auto elem_type = arr ? arr.ref(a)->element_type : f.ref(a)->type;
        auto width = lowering::byte_width(lc, elem_type);
        if (!width) {
            return unexpect_loc_error(f, "バイト幅の決まらない型");
        }
        auto buf_name = derived_name("buf", f);
        Node<Expr> nbytes;
        if (!arr) {
            nbytes = b.lit(*width);
            w.writeln("var ", buf_name, " [", std::to_string(*width), "]byte");
            locals[buf_name] = std::format("[{}]uint8", *width);
        }
        else if (!slice) {
            auto n = *const_uint(a, tables, count) * *width;
            nbytes = b.lit(n);
            w.writeln("var ", buf_name, " [", std::to_string(n), "]byte");
            locals[buf_name] = std::format("[{}]uint8", n);
        }
        else {
            MAYBE(n, expr(count));
            MAYBE(tn, type_name(f.ref(a)->type));
            if (decode) {
                room(w, n, *width);
                w.writeln(target_text, " = make(", tn, ", int(", n, "))");
            }
            else {
                w.writeln("if len(", target_text, ") != int(", n, ") {");
                {
                    auto s2 = w.indent_scope();
                    w.writeln("return nastAssert(", go_string("len(" + std::string(name_of(a, f)) + ")"), ")");
                }
                w.writeln("}");
            }
            w.writeln(buf_name, " := make([]byte, int(", n, ")*", std::to_string(*width), ")");
            locals[buf_name] = "[]uint8";
            nbytes = b.bin(BinaryOp::mul, b.paren(count), b.lit(*width), b.int_type(64));
        }
        auto buf = b.ref(buf_name, nullref);
        if (decode) {
            MAYBE_VOID(r, body(w, lowering::read_bytes(lc, lowering::input_stream(lc, b.loc), buf, nullref, nbytes)));
            check_read(w);
            auto lowered = lowering::lower_field_decode(lc, f, target, buf, nullref);
            if (!lowered) {
                return unexpect_loc_error(f, "field_io が組めない");
            }
            return body(w, lowered);
        }
        if (name_of(a, f).empty()) {
            // 書く値は期待値。無ければ 0 のまま。
            if (auto ex = field_args(a, f).expected; ex && !arr) {
                MAYBE(v, expr(ex));
                w.writeln(target_text, " = ", convert(v, type_of(ex), type_of(target)));
            }
        }
        auto lowered = lowering::lower_field_encode(lc, f, target, buf, nullref);
        if (!lowered) {
            return unexpect_loc_error(f, "field_io が組めない");
        }
        MAYBE_VOID(r, body(w, lowered));
        return body(w, lowering::write_bytes(lc, lowering::output_stream(lc, b.loc), buf, nullref, nbytes));
    }

    expected<void> Go::field(CodeWriter& w, Node<Field> f) {
        if (!is_layout_field(a, f)) {
            return {};
        }
        if (auto* pl = layout.find(f)) {
            switch (pl->kind) {
                case lowering::BitPlace::Kind::refused:
                    return unexpect_loc_error(f, "ビット群: {}", pl->why ? pl->why : "組めない");
                case lowering::BitPlace::Kind::flat:
                    if (pl->member == 0) {
                        return flat_group(w, layout.flat[pl->group]);
                    }
                    return {};
                case lowering::BitPlace::Kind::routed:
                    return routed_member(w, pl->group, pl->member);
            }
        }
        return plain_field(w, f);
    }

    expected<void> Go::branch(CodeWriter& w, Node<ConditionalExpr> cond) {
        if (auto m = cond.as_any<Match>()) {
            auto lowered = lowering::lower_match(lc, m);
            if (!lowered) {
                return unexpect_loc_error(cond, "match を if に落とせない");
            }
            cond = lowered;
        }
        bool first = true;
        for (auto& blk : cond.ref(a)->blocks) {
            auto cs = blk.as_any<ConditionalStatement>();
            if (cs && !is_default_cond(a, cs.ref(a)->condition)) {
                MAYBE(c, expr(cs.ref(a)->condition));
                w.writeln(first ? "if " : "} else if ", c, " {");
            }
            else {
                if (first) {
                    w.writeln("{");
                }
                else {
                    w.writeln("} else {");
                }
            }
            first = false;
            auto s = w.indent_scope();
            MAYBE_VOID(r, codec(w, blk.ref(a)->body));
        }
        if (!first) {
            w.writeln("}");
        }
        return {};
    }

    // format の Body (と分岐の腕) を読み書きの並びとして綴る。
    expected<void> Go::codec(CodeWriter& w, Node<Body> b) {
        if (!b) {
            return {};
        }
        for (auto& s : b.ref(a)->statements) {
            if (auto f = s.as_any<Field>()) {
                MAYBE_VOID(r, field(w, f));
                continue;
            }
            if (auto c = s.as_any<ConditionalExpr>()) {
                MAYBE_VOID(r, branch(w, c));
                continue;
            }
            if (s.as_any<VariableDefinition>() || s.as_any<Assert>()) {
                MAYBE_VOID(r, stmt(w, s));
                continue;
            }
            if (s.as_any<SpecifyOrder>()) {
                // 順は field ごとに FieldEndian に載っている。実行時に決まる順は
                // field_io が IsLittleEndian を組み、それを綴れないところで止まる。
                continue;
            }
            if (s.as_any<Metadata>()) {
                continue;
            }
            if (s.as_any<Function>() || s.as_any<Format>() || s.as_any<Enum>() || s.as_any<State>()) {
                continue;  // 宣言
            }
            return unexpect_loc_error(s, "{} は codec にまだ出せない", to_string(s.type()));
        }
        return {};
    }

    // struct のメンバを並び順に集める。分岐の腕の中の同名 field は、整数
    // どうしなら一番広い型に寄せる。
    expected<void> collect_members(Go& g, Node<Body> b, std::vector<std::string>& order) {
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
                    if (!is_go_int(it->second) || !is_go_int(t)) {
                        return unexpect_loc_error(f, "腕ごとに型の違う同名の field {}", name);
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
        Go g{.a = a, .tables = tables, .lc = lowering::Context{a, tables}, .fmt = fmt};
        auto* state = tables.table<FormatState>().get(fmt);
        if (state && (state->decode_kind == FormatKind::custom || state->encode_kind == FormatKind::custom)) {
            return unexpect_loc_error(fmt, "手書きの encode / decode");
        }
        auto name = exported(name_of(a, fmt));
        auto body = fmt.ref(a)->body;
        std::vector<std::string> order;
        MAYBE_VOID(m, collect_members(g, body, order));
        g.layout = lowering::bit_layout(g.lc, fmt);

        CodeWriter dec;
        g.decode = true;
        MAYBE_VOID(d, g.codec(dec, body));
        CodeWriter enc;
        g.decode = false;
        g.locals.clear();
        g.routed_declared.clear();
        MAYBE_VOID(e, g.codec(enc, body));

        CodeWriter w;
        w.writeln("type ", name, " struct {");
        {
            auto s = w.indent_scope();
            for (auto& n : order) {
                w.writeln(exported(n), " ", g.members[n]);
            }
        }
        w.writeln("}");
        w.writeln();
        w.writeln("func (t *", name, ") decode(d *nastDecoder) error {");
        {
            auto s = w.indent_scope();
            w.write(std::move(dec));
            w.writeln("return nil");
        }
        w.writeln("}");
        w.writeln();
        w.writeln("func (t *", name, ") encode(e *nastEncoder) error {");
        {
            auto s = w.indent_scope();
            w.write(std::move(enc));
            w.writeln("return nil");
        }
        w.writeln("}");
        w.writeln();
        w.writeln("func (t *", name, ") Decode(data []byte) (int, error) {");
        {
            auto s = w.indent_scope();
            w.writeln("d := &nastDecoder{data: data}");
            w.writeln("err := t.decode(d)");
            w.writeln("return d.off, err");
        }
        w.writeln("}");
        w.writeln();
        w.writeln("func (t *", name, ") Encode() ([]byte, error) {");
        {
            auto s = w.indent_scope();
            w.writeln("e := &nastEncoder{}");
            w.writeln("err := t.encode(e)");
            w.writeln("return e.out, err");
        }
        w.writeln("}");
        Emitted out{.w = std::move(w)};
        for (auto& [_, t] : g.members) {
            names_in(t, out.uses);
            if (!t.starts_with("[]")) {
                names_in(t, out.value_uses);
            }
        }
        return out;
    }

    expected<CodeWriter> emit_enum(Arena& a, SideTables& tables, Node<Enum> en) {
        Go g{.a = a, .tables = tables, .lc = lowering::Context{a, tables}};
        auto r = en.ref(a);
        if (!r->base_type) {
            return unexpect_loc_error(en, "下地の型の無い enum");
        }
        MAYBE(base, g.type_name(r->base_type));
        auto name = exported(name_of(a, en));
        CodeWriter w;
        w.writeln("type ", name, " ", base);
        w.writeln();
        w.writeln("const (");
        {
            auto s = w.indent_scope();
            for (auto& m : r->members) {
                auto mr = m.ref(a);
                auto v = const_uint(a, tables, mr->value);
                if (!v) {
                    return unexpect_loc_error(m, "値の決まらない enum のメンバ");
                }
                if (auto bits = int_bits_of(base); bits < 64 && *v >> bits) {
                    return unexpect_loc_error(m, "enum のメンバの値 {} が下地の {} に収まらない", *v, base);
                }
                w.writeln(name, exported(name_of(a, m)), " ", name, " = ", std::to_string(*v));
            }
        }
        w.writeln(")");
        return w;
    }

    constexpr auto runtime = R"(type nastDecoder struct {
	data []byte
	off  int
	err  error
}

func (d *nastDecoder) get() uint8 {
	if d.err != nil {
		return 0
	}
	if d.off >= len(d.data) {
		d.err = io.ErrUnexpectedEOF
		return 0
	}
	b := d.data[d.off]
	d.off++
	return b
}

func (d *nastDecoder) room(n uint64, width uint64) bool {
	return n <= uint64(len(d.data)-d.off)/width
}

func (d *nastDecoder) rest() []byte {
	out := append([]byte(nil), d.data[d.off:]...)
	d.off = len(d.data)
	return out
}

type nastEncoder struct {
	out []byte
}

func (e *nastEncoder) put(b uint8) {
	e.out = append(e.out, b)
}

func nastAssert(what string) error {
	return errors.New("assertion failed: " + what)
}

func nastBoolToInt[T ~uint8 | ~uint16 | ~uint32 | ~uint64](b bool) T {
	if b {
		return 1
	}
	return 0
}
)";

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
        auto each_decl = [&](auto&& self, Node<Body> b) -> void {
            for (auto& s : b.ref(a)->statements) {
                std::string name(name_of(a, s));
                auto f = s.as_any<Format>();
                auto en = s.as_any<Enum>();
                if (!f && !en) {
                    continue;
                }
                if (!seen.insert(exported(name)).second) {
                    continue;
                }
                Decl d{.name = exported(name)};
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
        auto top = a.make<Body>(n.ref(a).loc());
        top->statements = n.ref(a)->statements;
        each_decl(each_decl, top);

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
                    d.skipped = "値で自分を含む (Go の型にならない)";
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
                        d.skipped = it == by_name.end() ? std::format("宣言の無い型 {} を使う", u)
                                                        : std::format("飛ばした {} を使う", u);
                        d.out.reset();
                        changed = true;
                        break;
                    }
                }
            }
        }
        CodeWriter body;
        // トップレベルの定数 (`MAGIC ::= 0x...`)。型を付けない Go の定数にして、
        // 使う側の型に合わせさせる。整数に畳めたものだけ。
        for (auto& s : n.ref(a)->statements) {
            auto def = s.as_any<VariableDefinition>();
            if (!def) {
                continue;
            }
            auto name = local_name(name_of(a, def));
            if (auto v = const_uint(a, tables, def.ref(a)->value)) {
                body.writeln();
                body.writeln("const ", name, " = ", std::to_string(*v));
            }
            else {
                // `TypeA ::= u16(1)` のような型つきの定数。式のまま Go の定数にする。
                Go g{.a = a, .tables = tables, .lc = lowering::Context{a, tables}};
                auto text = g.expr(def.ref(a)->value);
                // 定数どうしの参照が自分へ戻るものは Go で初期化できない。
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
                if (text && !cyclic) {
                    // 三項 (即時の関数) は Go の定数にならないので var にする。
                    body.writeln();
                    body.writeln(text->find("func()") == std::string::npos ? "const " : "var ", name, " = ", *text);
                }
                else if (cyclic) {
                    futils::wrap::cerr_wrap() << "nast2go: 定数 " << name << " を飛ばした: 自分を参照する\n";
                }
                else {
                    futils::wrap::cerr_wrap() << "nast2go: 定数 " << name << " を飛ばした: " << text.error().message << '\n';
                }
            }
        }
        for (auto& d : decls) {
            body.writeln();
            if (d.out) {
                body.write(std::move(d.out->w));
                continue;
            }
            auto msg = std::format("{} を飛ばした: {}", d.name, d.skipped);
            futils::wrap::cerr_wrap() << "nast2go: " << msg << '\n';
            body.writeln("// nast2go: ", msg);
        }
        CodeWriter w;
        w.writeln("// Code generated by nast2go. DO NOT EDIT.");
        w.writeln("package ", c.lang_config().package);
        w.writeln();
        w.writeln("import (");
        {
            auto s = w.indent_scope();
            w.writeln("\"errors\"");
            w.writeln("\"io\"");
        }
        w.writeln(")");
        w.writeln();
        std::string_view rt = runtime;
        while (!rt.empty()) {
            auto nl = rt.find('\n');
            w.writeln(rt.substr(0, nl));
            rt = nl == std::string_view::npos ? std::string_view{} : rt.substr(nl + 1);
        }
        w.write(std::move(body));
        return w;
    });
    return {};
}
