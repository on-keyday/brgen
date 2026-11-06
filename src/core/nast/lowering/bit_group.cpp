/*license*/
#include "bit_group.hpp"
#include "field_io.hpp"
#include "int_bytes.hpp"
#include "../node/build.h"
#include "../node/util.h"

#include <algorithm>
#include <map>

namespace brgen::nast::lowering {

    namespace {

        // 式が入出力に触れうるか。呼び出し (input.get() も fn も) と、
        // input / output そのもの (input.offset のような位置の読み) を数える。
        // 群の途中で位置を読むと、どこを指すかが群をまとめて読む形と食い違う。
        bool touches_io(Arena& a, NodeAny n) {
            if (!is_side_effect_free(a, n)) {
                return true;
            }
            bool found = false;
            visit_all(a, n, [&](NodeAny x) {
                if (found) {
                    return false;
                }
                if (auto sl = x.as_any<SpecialLiteral>()) {
                    auto k = sl.ref(a)->kind;
                    if (k == SpecialLiteralKind::input_ || k == SpecialLiteralKind::output_) {
                        found = true;
                        return false;
                    }
                }
                return true;
            });
            return found;
        }

        // 群を閉じずに通してよい文。読み書きをしないもの: 入出力に触れない
        // 検査・定義・config・順の指定と、宣言 (fn / 入れ子の format / enum /
        // state)。順の指定は群を閉じないが、群の中で順が変われば order_of が
        // 断る。return / break / continue はここに入らない — 当たった経路は
        // 閉じない側に数える。
        bool is_transparent(Context& c, Node<Statement> s) {
            switch (s.type()) {
                case NodeType::Assert:
                case NodeType::VariableDefinition:
                case NodeType::Metadata:
                case NodeType::SpecifyOrder:
                    return !touches_io(c.a, s);
                case NodeType::Field:
                    return false;
                default:
                    return is_derived<NamedStatement>(s.type());
            }
        }

        // 既定の腕の無い match が、照合する値の取りうる値を全部腕で覆って
        // いるか。覆っていれば「どの腕にも入らない」経路は無い。値の範囲は
        // 型の幅から取り、腕は定数と定数の範囲だけを数える (それ以外が
        // 混ざれば覆っているとは言わない)。幅は 16 ビットまで。
        bool is_exhaustive(Context& c, Node<Match> m) {
            auto& a = c.a;
            auto cond = m.ref(a)->condition;
            if (!cond) {
                return false;
            }
            auto t = strip_wrappers(a, cond.ref(a)->type);
            if (auto e = t.as_any<EnumType>()) {
                auto base = e.ref(a)->base;
                t = base ? strip_wrappers(a, base.ref(a)->base_type) : Node<Type>{};
            }
            auto it = t.as_any<IntType>();
            if (!it || it.ref(a)->is_signed || it.ref(a)->bit_size > 16) {
                return false;
            }
            std::uint64_t span = std::uint64_t(1) << it.ref(a)->bit_size;
            std::vector<bool> covered(span, false);
            for (auto& blk : m.ref(a)->blocks) {
                auto cs = blk.as_any<ConditionalStatement>();
                if (!cs) {
                    return false;
                }
                auto p = strip_paren(a, cs.ref(a)->condition);
                std::uint64_t lo = 0, hi = 0;
                if (auto r = p.as_any<Range>()) {
                    auto s = const_uint(a, c.tables, r.ref(a)->start);
                    auto e = const_uint(a, c.tables, r.ref(a)->end);
                    if (!s || !e) {
                        return false;
                    }
                    lo = *s;
                    hi = r.ref(a)->op == BinaryOp::range_inclusive ? *e : *e - 1;
                    if (*e == 0 && r.ref(a)->op != BinaryOp::range_inclusive) {
                        continue;  // 空の範囲
                    }
                }
                else if (auto v = const_uint(a, c.tables, p)) {
                    lo = hi = *v;
                }
                else {
                    return false;
                }
                for (auto v = lo; v <= hi && v < span; v++) {
                    covered[v] = true;
                }
            }
            for (bool b : covered) {
                if (!b) {
                    return false;
                }
            }
            return true;
        }

        std::optional<std::size_t> fixed_bits(Context& c, Node<Field> f) {
            auto type = f.ref(c.a)->type;
            auto* sz = type ? c.tables.table<TypeSize>().get(type) : nullptr;
            if (!sz || sz->kind != SizeKind::fixed) {
                return std::nullopt;
            }
            return sz->bits;
        }

        // field 1 つ分の材料。読み書きに使う型と、期待値。
        struct Member {
            Node<Field> field;
            Node<Type> type;   // field の型 (enum ならその enum)
            bool is_enum = false;
            std::size_t bits = 0;
            std::size_t shift = 0;
            Node<Expr> expected;
        };

        // 組める群かを確かめて、field ごとの材料を返す。組めなければ空。
        // 群をどの順で読むか。byte は 2 バイト以上のときの整数の読み方、lsb は
        // 先頭の field を下位に置くか。
        struct Order {
            Endian byte = Endian::big;
            bool lsb = false;
        };

        // 群の順を決める。field ごとの FieldEndian が全員一致していること。
        //
        // bit_order の指定が無ければバイト順に従う (big は msb から、little は
        // lsb から)。既存の生成器は全部この対角の 2 象限で動いている (ebmgen は
        // 2 軸を 1 つの Endian に潰していて、bit_manipulator の shift が
        // `big ? 8 - n - off : off`)。1 バイトの群はバイト順が効かないので
        // bit_order だけで決まる。2 バイト以上で対角でない組み合わせ
        // (LE×MSB / BE×LSB) は組まない — ADR 0046 で very_slow_bit_ops の側に
        // 置いたもの。
        std::optional<Order> order_of(Context& c, const BitGroup& g, const char** why) {
            const FieldEndian* first = nullptr;
            for (auto f : g.fields) {
                auto* fe = c.tables.table<FieldEndian>().get(f);
                if (!fe) {
                    *why = "バイト順の表に無い field";
                    return std::nullopt;
                }
                if (!first) {
                    first = fe;
                    continue;
                }
                if (fe->endian != first->endian || fe->dynamic != first->dynamic ||
                    fe->bit_order != first->bit_order || fe->bit_order_dynamic != first->bit_order_dynamic) {
                    *why = "群の中で順が変わる";
                    return std::nullopt;
                }
            }
            if (!first) {
                *why = "field が無い";
                return std::nullopt;
            }
            if (first->bit_order_dynamic) {
                *why = "bit_order が実行時に決まる";
                return std::nullopt;
            }
            bool multi = g.bits > 8;
            bool byte_known = !first->dynamic && first->endian != Endian::native;
            if (multi && !byte_known) {
                *why = "2 バイト以上でバイト順が実行時かターゲットで決まる";
                return std::nullopt;
            }
            Order o;
            o.byte = first->endian == Endian::little ? Endian::little : Endian::big;
            if (first->bit_order != BitOrder::unspec) {
                o.lsb = first->bit_order == BitOrder::lsb;
                if (multi && o.lsb != (o.byte == Endian::little)) {
                    *why = "2 バイト以上でバイト順とビット順が対角でない";
                    return std::nullopt;
                }
                return o;
            }
            if (!byte_known) {
                *why = "ビット順がバイト順に従い、そのバイト順が実行時かターゲットで決まる";
                return std::nullopt;
            }
            o.lsb = o.byte == Endian::little;
            return o;
        }

        // field 1 つを群の一員として読み書きできるか。整数と enum だけ。
        std::optional<Member> member_of(Context& c, Node<Field> f, const char** why) {
            auto& a = c.a;
            auto refuse = [&](const char* reason) -> std::optional<Member> {
                *why = reason;
                return std::nullopt;
            };
            if (!is_layout_field(a, f)) {
                return refuse("読み書きの対象でない field");
            }
            auto args = field_args(a, f);
            if (args.named || args.positional > 1) {
                return refuse("field の引数");
            }
            Member m{.field = f, .expected = args.expected};
            m.type = strip_wrappers(a, f.ref(a)->type);
            if (auto e = m.type.as_any<EnumType>()) {
                m.is_enum = true;
                auto base = e.ref(a)->base;
                if (!base) {
                    return refuse("enum の下地が無い");
                }
                auto io = strip_wrappers(a, base.ref(a)->base_type).as_any<IntType>();
                if (!io || io.ref(a)->is_signed) {
                    return refuse("enum の下地が符号なし整数でない");
                }
                m.bits = io.ref(a)->bit_size;
            }
            else if (auto i = m.type.as_any<IntType>()) {
                if (i.ref(a)->is_signed) {
                    return refuse("符号つき");  // 符号拡張がまだ
                }
                m.bits = i.ref(a)->bit_size;
            }
            else {
                return refuse(m.type ? "整数と enum 以外の field" : "型の無い field");  // 入れ子 format / 配列
            }
            return m;
        }

        std::vector<Member> members_of(Context& c, const BitGroup& g, const char** why, Order* order) {
            auto refuse = [&](const char* reason) {
                *why = reason;
                return std::vector<Member>{};
            };
            auto& a = c.a;
            if (g.close != BitGroup::Close::flat || g.bits == 0 || g.bits % 8 != 0) {
                return refuse("平らでない");
            }
            if (g.bits > 64) {
                return refuse("64 ビットを超える");
            }
            auto o = order_of(c, g, why);
            if (!o) {
                return {};
            }
            *order = *o;
            std::vector<Member> ms;
            std::size_t used = 0;
            for (auto f : g.fields) {
                auto m = member_of(c, f, why);
                if (!m) {
                    return {};
                }
                used += m->bits;
                ms.push_back(*m);
            }
            if (used != g.bits) {
                return refuse("幅の表と型の幅が食い違う");
            }
            // msb なら先頭の field が上位、lsb なら下位。
            std::size_t pos = 0;
            for (auto& m : ms) {
                m.shift = o->lsb ? pos : g.bits - pos - m.bits;
                pos += m.bits;
            }
            return ms;
        }

        std::uint64_t mask_of(std::size_t bits) {
            return bits >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
        }

    }  // namespace

    const char* bit_group_refusal(Context& c, const BitGroup& g) {
        const char* why = nullptr;
        Order o;
        members_of(c, g, &why, &o);
        return why;
    }

    std::vector<BitGroup> bit_groups(Context& c, Node<Body> body) {
        std::vector<BitGroup> out;
        if (!body) {
            return out;
        }
        BitGroup cur;
        auto close = [&](BitGroup::Close how) {
            if (cur.fields.empty()) {
                return;
            }
            cur.close = how;
            out.push_back(std::move(cur));
            cur = {};
        };
        for (auto& s : body.ref(c.a)->statements) {
            if (is_transparent(c, s)) {
                continue;
            }
            auto fld = s.as<Field>();
            if (!fld) {
                close(is_derived<ConditionalExpr>(s.type()) ? BitGroup::Close::branch
                                                            : BitGroup::Close::other_statement);
                continue;
            }
            auto bits = fixed_bits(c, fld);
            if (!bits) {
                close(BitGroup::Close::variable_width);
                continue;
            }
            if (cur.fields.empty() && *bits % 8 == 0) {
                continue;  // バイト境界に乗る普通の field
            }
            cur.fields.push_back(fld);
            cur.bits += *bits;
            if (cur.bits % 8 == 0) {
                close(BitGroup::Close::flat);
            }
        }
        close(BitGroup::Close::body_end);
        return out;
    }

    std::vector<BitRoute> bit_routes(Context& c, Node<Body> body, std::size_t start) {
        auto& a = c.a;
        struct Frame {
            Node<Body> body;
            std::size_t index;
        };
        struct State {
            std::vector<Frame> frames;
            BitRoute route;
        };
        std::vector<BitRoute> done;
        std::vector<State> work;
        work.push_back(State{.frames = {Frame{body, start}}});
        auto finish = [&](State& st, BitRoute::End end) {
            st.route.end = end;
            done.push_back(std::move(st.route));
        };
        while (!work.empty()) {
            auto st = std::move(work.back());
            work.pop_back();
            while (true) {
                if (st.frames.empty()) {
                    finish(st, BitRoute::End::body_end);
                    break;
                }
                auto& top = st.frames.back();
                auto& stmts = top.body.ref(a)->statements;
                if (top.index >= stmts.size()) {
                    st.frames.pop_back();  // 腕の終わり。分岐の後へ戻る
                    continue;
                }
                auto s = stmts[top.index++];
                if (is_transparent(c, s)) {
                    continue;
                }
                if (auto fld = s.as<Field>()) {
                    auto bits = fixed_bits(c, fld);
                    if (!bits) {
                        finish(st, BitRoute::End::variable_width);
                        break;
                    }
                    st.route.fields.push_back(fld);
                    st.route.bits += *bits;
                    if (st.route.bits > 64) {
                        finish(st, BitRoute::End::too_long);
                        break;
                    }
                    if (st.route.bits % 8 == 0) {
                        finish(st, BitRoute::End::closed);
                        break;
                    }
                    continue;
                }
                if (auto cond = s.as_any<ConditionalExpr>()) {
                    bool has_default = false;
                    for (auto& blk : cond.ref(a)->blocks) {
                        auto cs = blk.as_any<ConditionalStatement>();
                        if (!cs || is_default_cond(a, cs.ref(a)->condition)) {
                            has_default = true;
                        }
                        auto fork = st;
                        fork.frames.push_back(Frame{blk.ref(a)->body, 0});
                        work.push_back(std::move(fork));
                    }
                    if (!has_default) {
                        if (auto m = cond.as_any<Match>(); m && is_exhaustive(c, m)) {
                            has_default = true;
                        }
                    }
                    if (has_default) {
                        break;  // 全経路が腕に入った
                    }
                    continue;  // 既定の腕が無い: 何も読まずに分岐の後へ
                }
                if (s.as_any<Loop>() || s.as_any<RangeLoop>()) {
                    finish(st, BitRoute::End::loop);
                    break;
                }
                finish(st, BitRoute::End::other_statement);
                break;
            }
        }
        return done;
    }

    Node<Body> lower_bit_group_decode(Context& c, const BitGroup& g, std::span<const Node<Expr>> targets,
                                      Node<Expr> bytes, Node<Expr> offset) {
        auto& a = c.a;
        if (targets.size() != g.fields.size()) {
            return nullref;
        }
        const char* why = nullptr;
        Order order;
        auto ms = members_of(c, g, &why, &order);
        if (ms.empty()) {
            return nullref;
        }
        Builder b{a, g.fields.front().ref(a).loc()};
        auto raw = b.int_type(g.bits);
        auto whole = combine_int(c, bytes, offset, raw, order.byte);
        auto def = b.define(derived_name("bits", g.fields.front()), whole);
        if (!def) {
            return nullref;
        }
        auto body = b.body(def);
        auto& stmts = body.ref(a)->statements;
        auto value = b.ref(def.ref(a)->name.ref(a)->identifier, raw);
        for (std::size_t i = 0; i < ms.size(); i++) {
            auto& m = ms[i];
            if (!targets[i]) {
                continue;
            }
            Builder fb = b.at(m.field.ref(a).loc());
            Node<Expr> part = value;
            if (m.shift) {
                part = fb.bin(BinaryOp::right_logical_shift, part, fb.lit(m.shift), raw);
            }
            part = fb.bin(BinaryOp::bit_and, part, fb.lit(mask_of(m.bits)), raw);
            auto assign = fb.assign(targets[i], fb.cast(m.type, part));
            if (!assign) {
                return nullref;
            }
            stmts.push_back(assign);
            if (m.expected) {
                if (!add_expected_check(c, targets[i], body, m.expected, true)) {
                    return nullref;
                }
            }
        }
        return body;
    }

    Node<Body> lower_bit_group_encode(Context& c, const BitGroup& g, std::span<const Node<Expr>> targets,
                                      Node<Expr> bytes, Node<Expr> offset) {
        auto& a = c.a;
        if (targets.size() != g.fields.size()) {
            return nullref;
        }
        const char* why = nullptr;
        Order order;
        auto ms = members_of(c, g, &why, &order);
        if (ms.empty()) {
            return nullref;
        }
        Builder b{a, g.fields.front().ref(a).loc()};
        auto raw = b.int_type(g.bits);
        Node<Expr> acc;
        std::vector<std::pair<Node<Expr>, Node<Expr>>> checks;
        for (std::size_t i = 0; i < ms.size(); i++) {
            auto& m = ms[i];
            Builder fb = b.at(m.field.ref(a).loc());
            Node<Expr> v = targets[i];
            if (!v) {
                v = m.expected;
                if (!v) {
                    continue;  // 書き先も期待値も無い = 0
                }
            }
            else if (m.expected) {
                checks.emplace_back(targets[i], m.expected);
            }
            Node<Expr> part = fb.bin(BinaryOp::bit_and, fb.cast(raw, v), fb.lit(mask_of(m.bits)), raw);
            if (m.shift) {
                part = fb.bin(BinaryOp::left_logical_shift, part, fb.lit(m.shift), raw);
            }
            acc = fb.join(BinaryOp::bit_or, acc, part, raw);
        }
        if (!acc) {
            acc = b.lit(0);
        }
        // split_int は値の式をバイトごとに繰り返すので、先に 1 度だけ組む。
        auto def = b.define(derived_name("bits", g.fields.front()), acc);
        if (!def) {
            return nullref;
        }
        auto body = split_int(c, bytes, offset, b.ref(def.ref(a)->name.ref(a)->identifier, raw), raw, order.byte);
        if (!body) {
            return nullref;
        }
        auto& stmts = body.ref(a)->statements;
        stmts.insert(stmts.begin(), def);
        // 検査は書く前。add_expected_check が先頭に積むので逆順に足す。
        for (auto it = checks.rbegin(); it != checks.rend(); ++it) {
            if (!add_expected_check(c, it->first, body, it->second, false)) {
                return nullref;
            }
        }
        return body;
    }

    std::optional<RoutedBitGroup> routed_bit_group(Context& c, Node<Body> body, std::size_t start, const char** why) {
        auto routes = bit_routes(c, body, start);
        if (routes.empty()) {
            *why = "経路が無い";
            return std::nullopt;
        }
        // field ごとの位置。経路で変われば静的には組めない。
        std::map<std::uint32_t, RoutedBitField> by_field;
        BitGroup all;  // order_of に渡す和集合
        for (auto& r : routes) {
            if (r.end != BitRoute::End::closed) {
                *why = "閉じない経路がある";
                return std::nullopt;
            }
            all.bits = std::max(all.bits, r.bits);
            std::size_t pos = 0;
            for (auto f : r.fields) {
                auto m = member_of(c, f, why);
                if (!m) {
                    return std::nullopt;
                }
                auto [it, fresh] = by_field.try_emplace(f.id(), RoutedBitField{.field = f, .offset = pos, .bits = m->bits});
                if (!fresh && it->second.offset != pos) {
                    *why = "位置が経路で変わる";
                    return std::nullopt;
                }
                if (fresh) {
                    all.fields.push_back(f);
                }
                pos += m->bits;
            }
        }
        all.close = BitGroup::Close::flat;
        auto o = order_of(c, all, why);
        if (!o) {
            return std::nullopt;
        }
        RoutedBitGroup g;
        g.lsb = o->lsb;
        g.max_bytes = all.bits / 8;
        for (auto& [_, f] : by_field) {
            auto end = f.offset + f.bits;
            // 読む前: 手前の field が offset まで読ませているので、その続きから end まで。
            f.fetch_from = (f.offset + 7) / 8;
            f.fetch_to = (end + 7) / 8;
            // 書いた後: offset の手前までのバイトは前の field で書き終わっている。
            f.flush_from = f.offset / 8;
            f.flush_to = end / 8;
            if (end - f.offset / 8 * 8 > 64) {
                *why = "field が 8 バイトを超える範囲に跨る";
                return std::nullopt;
            }
            g.fields.push_back(f);
        }
        std::ranges::sort(g.fields, [](auto& l, auto& r) {
            return l.offset != r.offset ? l.offset < r.offset : l.field.id() < r.field.id();
        });
        return g;
    }

    namespace {
        // field が乗っているバイト [first, first + n) と、その n バイトを 1 つの
        // 整数に見たときの field の下端の位置。
        struct Span {
            std::size_t first = 0;
            std::size_t n = 0;
            std::size_t shift = 0;
        };

        Span span_of(const RoutedBitGroup& g, const RoutedBitField& f) {
            Span s;
            s.first = f.offset / 8;
            auto last = (f.offset + f.bits - 1) / 8;
            s.n = last - s.first + 1;
            auto in_byte = f.offset % 8;
            // msb: n バイトをビッグで読んだ整数の上から in_byte 進んだところが
            // field の上端。lsb: リトルで読んだ整数の下から in_byte 進んだところが
            // field の下端。
            s.shift = g.lsb ? in_byte : s.n * 8 - in_byte - f.bits;
            return s;
        }

        Node<Expr> at_byte(Builder& b, Node<Expr> offset, std::size_t k) {
            if (!offset) {
                return b.lit(k);
            }
            if (k == 0) {
                return offset;
            }
            return b.bin(BinaryOp::add, offset, b.lit(k), b.int_type(64));
        }
    }  // namespace

    Node<Body> lower_routed_bit_decode(Context& c, const RoutedBitGroup& g, std::size_t index, Node<Expr> target,
                                       Node<Expr> bytes, Node<Expr> offset) {
        auto& a = c.a;
        if (index >= g.fields.size() || !target) {
            return nullref;
        }
        auto& f = g.fields[index];
        const char* why = nullptr;
        auto m = member_of(c, f.field, &why);
        if (!m) {
            return nullref;
        }
        auto sp = span_of(g, f);
        Builder b{a, f.field.ref(a).loc()};
        auto raw = b.int_type(sp.n * 8);
        auto whole = combine_int(c, bytes, at_byte(b, offset, sp.first), raw, g.lsb ? Endian::little : Endian::big);
        if (!whole) {
            return nullref;
        }
        Node<Expr> part = whole;
        if (sp.shift) {
            part = b.bin(BinaryOp::right_logical_shift, b.paren(part), b.lit(sp.shift), raw);
        }
        part = b.bin(BinaryOp::bit_and, part, b.lit(mask_of(f.bits)), raw);
        auto body = b.body(b.assign(target, b.cast(m->type, part)));
        return add_expected_check(c, target, body, m->expected, true);
    }

    Node<Body> lower_routed_bit_encode(Context& c, const RoutedBitGroup& g, std::size_t index, Node<Expr> target,
                                       Node<Expr> bytes, Node<Expr> offset) {
        auto& a = c.a;
        if (index >= g.fields.size()) {
            return nullref;
        }
        auto& f = g.fields[index];
        const char* why = nullptr;
        auto m = member_of(c, f.field, &why);
        if (!m) {
            return nullref;
        }
        Builder b{a, f.field.ref(a).loc()};
        Node<Expr> v = target ? target : m->expected;
        if (!v) {
            return b.body();  // 書き先も期待値も無い = 0 のまま
        }
        auto sp = span_of(g, f);
        auto raw = b.int_type(sp.n * 8);
        Node<Expr> part = b.bin(BinaryOp::bit_and, b.cast(raw, v), b.lit(mask_of(f.bits)), raw);
        if (sp.shift) {
            part = b.bin(BinaryOp::left_logical_shift, part, b.lit(sp.shift), raw);
        }
        auto def = b.define(derived_name("bits", f.field), part);
        if (!def) {
            return nullref;
        }
        auto body = b.body(def);
        auto& stmts = body.ref(a)->statements;
        auto value = b.ref(def.ref(a)->name.ref(a)->identifier, raw);
        auto u8 = b.int_type(8);
        for (std::size_t j = 0; j < sp.n; j++) {
            // msb はビッグ (先頭のバイトが上位)、lsb はリトル。
            auto byte_shift = g.lsb ? j * 8 : (sp.n - 1 - j) * 8;
            Node<Expr> piece = value;
            if (byte_shift) {
                piece = b.bin(BinaryOp::right_logical_shift, piece, b.lit(byte_shift), raw);
            }
            auto slot = b.index(bytes, at_byte(b, offset, sp.first + j), u8);
            stmts.push_back(b.assign(slot, b.bin(BinaryOp::bit_or, slot, b.cast(u8, piece), u8)));
        }
        if (target && m->expected) {
            return add_expected_check(c, target, body, m->expected, false);
        }
        return body;
    }

}  // namespace brgen::nast::lowering
