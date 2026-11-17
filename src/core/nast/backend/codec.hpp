/*license*/
#pragma once
#include "../lowering/bit_group.hpp"
#include "../lowering/field_io.hpp"
#include "../lowering/follow.hpp"
#include "../lowering/match_to_if.hpp"
#include "../lowering/self_ref.hpp"
#include "../node/build.h"
#include "../node/error.h"
#include "../node/util.h"
#include "../parse/unparse.h"

#include <set>

// format の Body を「読み書きの並び」として歩く、言語に依らない部分。
//
//   field ごとに lowering::bit_layout を引き、平らな群 / 分岐を跨ぐ群 /
//   1 つの値 / 配列 / 入れ子 format に振り分ける
//   分岐は if の連鎖 (match は lowering::lower_match)
//   定義と assert は並びのまま、宣言と順の指定は読み飛ばす
//
// 綴りは Derived (CRTP) が持つ。口は 2 種類:
//
//   既定のある口   scalar / array / flat_group / routed — lowering の文の形
//                  (lower_field_* / lower_bit_group_* / lower_routed_bit_*) を
//                  既定に持つ。言語が native な書き方を持っていれば上書きし、
//                  持っていない場合だけ既定に落とす (Walker::scalar を呼ぶ)
//   既定の無い口   バイトの出し入れ (acquire / declare_out / fetch_into / flush)、
//                  入れ子 format、末尾までの配列、分岐の綴り、文の綴り
//
// lowering の文の形はバッファと位置を呼ぶ側に任せている (field_io.hpp)。
// その「呼ぶ側」の仕事 (バイトを手元に揃える / 書いたバイトを出す) が
// 既定の無い口で、ここが入出力の口 (スライス / reader / append など) ごとに
// 変わる。本体を 1 つにして口ごとに展開するのは Derived の側。
//
// EBM は全部の形を IR に先に載せて言語に選ばせた。ここは形を組むのを
// 呼ばれたときまで遅らせ、どの形を使うかを口ごとに言語が決める。

namespace brgen::nast::backend::codec {

    template <class Derived>
    struct Walker {
        Arena& a;
        SideTables& tables;
        lowering::Context lc;
        bool decode = true;
        lowering::BitLayout layout;
        std::set<std::size_t> routed_started;
        // 今歩いている文の場所 (配列の後ろに何が来るかを引くため)。
        Node<Body> cur_body;
        std::size_t cur_index = 0;
        // decode: 手前の配列が終端として読み終えた field。その field 自身では読まない。
        std::set<std::uint32_t> consumed;
        // 部分範囲の中で読んでいる field。
        Node<Field> subrange_field;

        Walker(Arena& a, SideTables& t)
            : a(a), tables(t), lc{a, t} {}

        Derived& d() {
            return static_cast<Derived&>(*this);
        }

        Builder builder(NodeAny origin) {
            return Builder{a, origin.ref(a).loc()};
        }

        // fmt を 1 方向ぶん歩く。
        expected<void> run(Node<Format> fmt, bool dec) {
            decode = dec;
            layout = lowering::bit_layout(lc, fmt);
            routed_started.clear();
            consumed.clear();
            return body(fmt.ref(a)->body);
        }

        expected<void> body(Node<Body> b) {
            if (!b) {
                return {};
            }
            auto saved_body = cur_body;
            auto saved_index = cur_index;
            auto& stmts = b.ref(a)->statements;
            for (std::size_t i = 0; i < stmts.size(); i++) {
                cur_body = b;
                cur_index = i;
                auto r = statement(stmts[i]);
                if (!r) {
                    return r;
                }
            }
            cur_body = saved_body;
            cur_index = saved_index;
            return {};
        }

        expected<void> statement(Node<Statement> s) {
            if (auto f = s.as_any<Field>()) {
                return field(f);
            }
            if (auto c = s.as_any<ConditionalExpr>()) {
                return branch(c);
            }
            if (s.as_any<VariableDefinition>() || s.as_any<Assert>() || s.as_any<ExplicitError>()) {
                return d().emit_statement(s);
            }
            if (auto r = s.as_any<Range>(); r && !r.ref(a)->start && !r.ref(a)->end) {
                return {};  // `..` は空の文 (何もしない腕の置き場)
            }
            if (s.as_any<SpecifyOrder>() || s.as_any<Metadata>()) {
                // 順は field ごとに FieldEndian に載っている。実行時に決まる順は
                // scalar_shape が dynamic_order として渡す。
                return {};
            }
            if (s.as_any<Function>() || s.as_any<Format>() || s.as_any<Enum>() || s.as_any<State>()) {
                return {};  // 宣言
            }
            return unexpect_loc_error(s, "{} is not supported in an encode/decode body yet", to_string(s.type()));
        }

        // --- field の振り分け ------------------------------------------------

        expected<void> field(Node<Field> f) {
            if (!is_layout_field(a, f)) {
                return {};
            }
            if (decode && consumed.contains(f.id())) {
                return {};
            }
            if (auto* pl = layout.find(f)) {
                switch (pl->kind) {
                    case lowering::BitPlace::Kind::refused:
                        return unexpect_loc_error(f, "bit group: {}", pl->why ? pl->why : "refused");
                    case lowering::BitPlace::Kind::flat:
                        if (pl->member == 0) {
                            return d().flat_group(layout.flat[pl->group]);
                        }
                        return {};
                    case lowering::BitPlace::Kind::routed: {
                        if (!routed_started.contains(pl->group)) {
                            routed_started.insert(pl->group);
                            auto r = d().routed_begin(pl->group);
                            if (!r) {
                                return r;
                            }
                        }
                        return d().routed(pl->group, pl->member);
                    }
                }
            }
            if (auto len = lowering::subrange_length(lc, f)) {
                // `input = input.subrange(n)`: その field を長さ n の部分範囲の
                // 中で読み書きする。範囲の作り方は言語の口。
                auto saved = subrange_field;
                subrange_field = f;
                auto r = d().subrange(f, len, [&]() { return typed_field(f); });
                subrange_field = saved;
                return r;
            }
            return typed_field(f);
        }

        // 名前つきの引数の数。部分範囲の中で読んでいる field の `input = ...` は数えない
        // (subrange の口が受け持っている)。
        std::size_t named_args(Node<Field> f) {
            auto n = field_args(a, f).named;
            if (f == subrange_field && n > 0) {
                n--;
            }
            return n;
        }

        // 型で振り分ける。
        expected<void> typed_field(Node<Field> f) {
            auto type = strip_wrappers(a, f.ref(a)->type);
            if (auto s = type.as_any<StructType>(); s && s.ref(a)->base.as_any<Format>()) {
                if (named_args(f) || field_args(a, f).positional) {
                    return unexpect_loc_error(f, "nested format with arguments");
                }
                return d().nested(f, target_of(f));
            }
            if (auto arr = type.as_any<ArrayType>()) {
                return d().array(f, arr);
            }
            if (auto bytes = lowering::literal_bytes(lc, type)) {
                // 文字列の期待値 (`magic :"ustar\0"`)。読んで比べる / そのまま書く。
                return d().literal_field(f, *bytes, target_of(f));
            }
            if (auto shape = lowering::scalar_shape(lc, f, f.ref(a)->type)) {
                if (named_args(f) || field_args(a, f).positional > 1) {
                    return unexpect_loc_error(f, "named arguments or more than one expected value");
                }
                return d().scalar(f, *shape, target_of(f));
            }
            return unexpect_loc_error(f, "field of type {} is not supported yet", to_string(type.type()));
        }

        // 書き先。名前のある field は field_ref、無い field は Derived が局所変数を作る。
        // encode で名前の無い field は null (期待値か 0 を書く)。
        Node<Expr> target_of(Node<Field> f) {
            if (!name_of(a, f).empty()) {
                return lowering::field_ref(lc, f);
            }
            if (!decode) {
                return nullref;
            }
            return d().temp_target(f);
        }

        // --- 既定のある口: lowering の文の形 ---------------------------------

        // 1 つの値。バイトを手元に揃え (decode) / 書く場所を用意し (encode)、
        // lower_field_* の文を綴る。
        expected<void> scalar(Node<Field> f, const lowering::ScalarShape& s, Node<Expr> target) {
            return lowered_field(f, target, nullref, s.bytes);
        }

        // 配列。末尾まで / 入れ子 format の要素は既定を持たない。固定幅の要素は
        // 要素の数 * 幅のバイトを揃えて lower_field_* の文 (要素のループ) を綴る。
        expected<void> array(Node<Field> f, Node<ArrayType> arr) {
            auto len = arr.ref(a)->length;
            auto elem = strip_wrappers(a, arr.ref(a)->element_type);
            if (!len || len.as_any<Range>()) {
                // 長さの無い配列。どこまで読むかは後ろに何が来るかで決まる。
                auto follow = lowering::field_follow(lc, cur_body, cur_index);
                if (f == subrange_field) {
                    follow = lowering::FieldFollow{};  // 部分範囲の終わりまで
                }
                return d().array_until_end(f, arr, target_of(f), follow);
            }
            if (auto s = elem.as_any<StructType>(); s && s.ref(a)->base.as_any<Format>()) {
                return d().nested_array(f, arr, target_of(f));
            }
            auto width = lowering::byte_width(lc, arr.ref(a)->element_type);
            if (!width) {
                return unexpect_loc_error(f, "array element has no fixed byte width");
            }
            if (named_args(f)) {
                return unexpect_loc_error(f, "array field with named arguments");
            }
            auto target = target_of(f);
            auto b = builder(f);
            Node<Expr> count = len;
            if (auto n = const_uint(a, tables, len)) {
                return lowered_field(f, target, nullref, *n * *width);
            }
            // 実行時に決まる数。decode はバイトを先に揃えてから書き先を確保する
            // (入力から来た数で先に確保すると、入力が持っていない量を確保しうる)。
            // encode は数を確かめる。
            auto nbytes = b.bin(BinaryOp::mul, b.paren(count), b.lit(*width), b.int_type(64));
            if (!decode) {
                auto r = d().check_count(target, count);
                if (!r) {
                    return r;
                }
                return lowered_field(f, target, nbytes, 0);
            }
            auto buf = d().acquire(f, nbytes, 0);
            if (!buf) {
                return unexpect(std::move(buf.error()));
            }
            auto r = d().allocate(target, arr, count);
            if (!r) {
                return r;
            }
            auto lowered = lowering::lower_field_decode(lc, f, target, *buf, nullref);
            if (!lowered) {
                return unexpect_loc_error(f, "field_io cannot lower this field");
            }
            return d().emit_body(lowered);
        }

        // nbytes (式) か fixed_bytes (定数) のどちらか。
        expected<void> lowered_field(Node<Field> f, Node<Expr> target, Node<Expr> nbytes, std::size_t fixed_bytes) {
            if (decode) {
                auto buf = d().acquire(f, nbytes, fixed_bytes);
                if (!buf) {
                    return unexpect(std::move(buf.error()));
                }
                auto lowered = lowering::lower_field_decode(lc, f, target, *buf, nullref);
                if (!lowered) {
                    return unexpect_loc_error(f, "field_io cannot lower this field");
                }
                return d().emit_body(lowered);
            }
            auto buf = d().declare_out(f, nbytes, fixed_bytes);
            if (!buf) {
                return unexpect(std::move(buf.error()));
            }
            if (!target) {
                target = d().preset_target(f);
            }
            auto lowered = lowering::lower_field_encode(lc, f, target, *buf, nullref);
            if (!lowered) {
                return unexpect_loc_error(f, "field_io cannot lower this field");
            }
            auto r = d().emit_body(lowered);
            if (!r) {
                return r;
            }
            auto b = builder(f);
            return d().flush(*buf, 0, nbytes ? nbytes : b.lit(fixed_bytes));
        }

        std::vector<Node<Expr>> group_targets(const lowering::BitGroup& g) {
            std::vector<Node<Expr>> out;
            for (auto f : g.fields) {
                out.push_back(target_of(f));
            }
            return out;
        }

        expected<void> flat_group(const lowering::BitGroup& g) {
            auto first = g.fields.front();
            auto n = g.bits / 8;
            auto targets = group_targets(g);
            if (decode) {
                auto buf = d().acquire(first, nullref, n);
                if (!buf) {
                    return unexpect(std::move(buf.error()));
                }
                auto lowered = lowering::lower_bit_group_decode(lc, g, targets, *buf, nullref);
                if (!lowered) {
                    return unexpect_loc_error(first, "cannot lower the bit group");
                }
                return d().emit_body(lowered);
            }
            auto buf = d().declare_out(first, nullref, n);
            if (!buf) {
                return unexpect(std::move(buf.error()));
            }
            auto lowered = lowering::lower_bit_group_encode(lc, g, targets, *buf, nullref);
            if (!lowered) {
                return unexpect_loc_error(first, "cannot lower the bit group");
            }
            auto r = d().emit_body(lowered);
            if (!r) {
                return r;
            }
            return d().flush(*buf, 0, builder(first).lit(n));
        }

        // 分岐を跨ぐ群のバッファ。群の先頭の field の位置で 1 度だけ宣言する
        // (腕の中から使うので、分岐より外に要る)。
        Node<Expr> routed_buffer(std::size_t group) {
            auto first = layout.routed[group].fields.front().field;
            return builder(first).ref(derived_name("group", first));
        }

        expected<void> routed_begin(std::size_t group) {
            auto& g = layout.routed[group];
            return d().declare_group_buffer(routed_buffer(group), g.max_bytes);
        }

        expected<void> routed(std::size_t group, std::size_t member) {
            auto& g = layout.routed[group];
            auto& f = g.fields[member];
            auto b = builder(f.field);
            auto buf = routed_buffer(group);
            auto target = target_of(f.field);
            if (decode) {
                if (f.fetch_to > f.fetch_from) {
                    auto r = d().fetch_into(buf, f.fetch_from, f.fetch_to - f.fetch_from);
                    if (!r) {
                        return r;
                    }
                }
                auto lowered = lowering::lower_routed_bit_decode(lc, g, member, target, buf, nullref);
                if (!lowered) {
                    return unexpect_loc_error(f.field, "cannot lower the bit group across a branch");
                }
                return d().emit_body(lowered);
            }
            auto lowered = lowering::lower_routed_bit_encode(lc, g, member, target, buf, nullref);
            if (!lowered) {
                return unexpect_loc_error(f.field, "cannot lower the bit group across a branch");
            }
            auto r = d().emit_body(lowered);
            if (!r) {
                return r;
            }
            if (f.flush_to > f.flush_from) {
                return d().flush(buf, f.flush_from, b.lit(f.flush_to - f.flush_from));
            }
            return {};
        }

        // --- 手書きの encode / decode (fn) ------------------------------------
        //
        // `fn decode():` / `fn encode():` の本体を文の並びのまま綴る。言語に依らない
        // 部分はここで決める:
        //
        //   入出力の呼び出し (input.get() / input.get(T) / output.put(x)) は文の
        //   前に出し、式の中ではその結果の名前を読む。式の中の評価順は言語ごとに
        //   違うので任せない (lowering/stream_io.hpp と同じ理由)
        //   `xs[xs.length] = v` は末尾に足す
        //   式だけの文 (`i + 1 == labels.length`) は検査 (ADR 0033)
        //
        // 綴りの口: stream_get / stream_put / append_element / loop_begin /
        // range_begin / loop_end / flow (break / continue / return)。

        expected<void> run_custom(Node<Function> fn, bool dec) {
            decode = dec;
            routed_started.clear();
            consumed.clear();
            return fn_body(fn.ref(a)->body);
        }

        expected<void> fn_body(Node<Body> b) {
            if (!b) {
                return {};
            }
            for (auto& s : b.ref(a)->statements) {
                auto r = fn_statement(s);
                if (!r) {
                    return r;
                }
            }
            return {};
        }

        bool is_stream_call(Node<Expr> e, SpecialLiteralKind kind, std::string_view member) {
            auto call = strip_paren(a, e).as_any<Call>();
            if (!call) {
                return false;
            }
            auto callee = strip_paren(a, call.ref(a)->callee).as_any<MemberAccess>();
            if (!callee || ident_text(a, callee.ref(a)->member) != member) {
                return false;
            }
            auto base = strip_paren(a, callee.ref(a)->base).as_any<SpecialLiteral>();
            return base && base.ref(a)->kind == kind;
        }

        // root の中の input.get(...) を、現れる順に文の前へ出す。
        expected<void> hoist_gets(NodeAny root) {
            if (!root) {
                return {};
            }
            std::vector<Node<Call>> calls;
            visit_all(a, root, [&](NodeAny n) {
                if (n.as_any<Body>()) {
                    return false;  // 入れ子の文は自分の番で出す
                }
                if (auto c = n.as_any<Call>(); c && is_stream_call(c, SpecialLiteralKind::input_, "get")) {
                    calls.push_back(c);
                }
                return true;
            });
            for (auto c : calls) {
                auto r = d().stream_get(c);
                if (!r) {
                    return r;
                }
            }
            return {};
        }

        // `xs[xs.length] = v` の xs。そうでなければ null。
        Node<Expr> append_target(Node<Assign> as) {
            auto ix = strip_paren(a, as.ref(a)->assignee).as_any<Index>();
            if (!ix) {
                return nullref;
            }
            auto len = strip_paren(a, ix.ref(a)->index).as_any<MemberAccess>();
            if (!len || ident_text(a, len.ref(a)->member) != "length") {
                return nullref;
            }
            if (unparse_node(a, len.ref(a)->base) != unparse_node(a, ix.ref(a)->base)) {
                return nullref;
            }
            return ix.ref(a)->base;
        }

        expected<void> fn_statement(Node<Statement> s) {
            if (auto c = s.as_any<ConditionalExpr>()) {
                std::optional<expected<void>> bad;
                visit_all(a, c, [&](NodeAny n) {
                    if (n.as_any<Body>()) {
                        return false;
                    }
                    if (auto call = n.as_any<Call>(); call && is_stream_call(call, SpecialLiteralKind::input_, "get")) {
                        bad = unexpect_loc_error(n, "input.get() inside a branch condition");
                    }
                    return !bad;
                });
                if (bad) {
                    return *bad;
                }
                if (auto m = c.as_any<Match>()) {
                    auto lowered = lowering::lower_match(lc, m);
                    if (!lowered) {
                        return unexpect_loc_error(c, "cannot lower match to if");
                    }
                    c = lowered;
                }
                bool first = true;
                for (auto& blk : c.ref(a)->blocks) {
                    Node<Expr> cond;
                    if (auto cs = blk.as_any<ConditionalStatement>(); cs && !is_default_cond(a, cs.ref(a)->condition)) {
                        cond = cs.ref(a)->condition;
                    }
                    auto r = d().branch_arm(cond, first);
                    if (!r) {
                        return r;
                    }
                    first = false;
                    r = fn_body(blk.ref(a)->body);
                    if (!r) {
                        return r;
                    }
                }
                return first ? expected<void>{} : d().branch_end();
            }
            if (auto loop = s.as_any<Loop>()) {
                auto r = loop.ref(a);
                if (r->init || r->step) {
                    return unexpect_loc_error(s, "loop with init or step");
                }
                auto b = d().loop_begin(r->condition);
                if (!b) {
                    return b;
                }
                auto body_r = fn_body(r->body);
                if (!body_r) {
                    return body_r;
                }
                return d().loop_end();
            }
            if (auto loop = s.as_any<RangeLoop>()) {
                auto b = d().range_begin(loop.ref(a)->binding);
                if (!b) {
                    return b;
                }
                auto body_r = fn_body(loop.ref(a)->body);
                if (!body_r) {
                    return body_r;
                }
                return d().loop_end();
            }
            if (s.as_any<Break>() || s.as_any<Continue>() || s.as_any<Return>()) {
                return d().flow(s);
            }
            if (auto as = s.as_any<Assign>()) {
                auto r = hoist_gets(as.ref(a)->value);
                if (!r) {
                    return r;
                }
                if (auto base = append_target(as)) {
                    return d().append_element(base, as.ref(a)->value);
                }
                return d().emit_statement(s);
            }
            if (auto def = s.as_any<VariableDefinition>()) {
                auto r = hoist_gets(def.ref(a)->value);
                if (!r) {
                    return r;
                }
                return d().emit_statement(s);
            }
            if (s.as_any<Assert>() || s.as_any<ExplicitError>()) {
                auto r = hoist_gets(s);
                if (!r) {
                    return r;
                }
                return d().emit_statement(s);
            }
            if (s.as_any<Metadata>() || s.as_any<SpecifyOrder>()) {
                return {};
            }
            if (auto call = s.as_any<Call>()) {
                if (is_stream_call(call, SpecialLiteralKind::output_, "put")) {
                    auto args = call.ref(a)->arguments.ref(a);
                    if (args && !args->arguments.empty()) {
                        auto r = hoist_gets(args->arguments[0].ref(a)->value);
                        if (!r) {
                            return r;
                        }
                    }
                    return d().stream_put(call);
                }
                if (is_stream_call(call, SpecialLiteralKind::input_, "get")) {
                    return d().stream_get(call);  // 読み捨て
                }
            }
            if (auto e = s.as_any<Expr>()) {
                // 式だけの文は検査 (ADR 0033)。
                auto r = hoist_gets(e);
                if (!r) {
                    return r;
                }
                auto b = builder(e);
                return d().emit_statement(b.assert_(e));
            }
            return unexpect_loc_error(s, "{} is not supported in a custom encode / decode function yet", to_string(s.type()));
        }

        // --- 分岐 ------------------------------------------------------------

        expected<void> branch(Node<ConditionalExpr> cond) {
            if (auto m = cond.as_any<Match>()) {
                auto lowered = lowering::lower_match(lc, m);
                if (!lowered) {
                    return unexpect_loc_error(cond, "cannot lower match to if");
                }
                cond = lowered;
            }
            bool first = true;
            for (auto& blk : cond.ref(a)->blocks) {
                Node<Expr> c;
                if (auto cs = blk.as_any<ConditionalStatement>(); cs && !is_default_cond(a, cs.ref(a)->condition)) {
                    c = cs.ref(a)->condition;
                }
                auto r = d().branch_arm(c, first);
                if (!r) {
                    return r;
                }
                first = false;
                r = body(blk.ref(a)->body);
                if (!r) {
                    return r;
                }
            }
            if (!first) {
                return d().branch_end();
            }
            return {};
        }
    };

}  // namespace brgen::nast::backend::codec
