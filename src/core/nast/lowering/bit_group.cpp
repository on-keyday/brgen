/*license*/
#include "bit_group.hpp"
#include "field_io.hpp"
#include "int_bytes.hpp"
#include "../node/build.h"
#include "../node/util.h"

namespace brgen::nast::lowering {

    namespace {

        // 群を閉じずに通してよい文。読み書きをしないもの。
        bool is_transparent(NodeType t) {
            switch (t) {
                case NodeType::Assert:
                case NodeType::VariableDefinition:
                case NodeType::Metadata:
                    return true;
                default:
                    return false;
            }
        }

        std::optional<std::size_t> fixed_bits(Context& c, Node<Field> f) {
            auto type = f.ref(c.a)->type;
            auto* sz = type ? c.tables.table<TypeSize>().get(type) : nullptr;
            if (!sz || sz->kind != SizeKind::fixed) {
                return std::nullopt;
            }
            return sz->bits;
        }

        bool has_bit_order(Context& c) {
            bool found = false;
            each_node<SpecifyOrder>(c.a, [&](Node<SpecifyOrder> s) {
                if (s.ref(c.a)->name.ends_with("bit_order")) {
                    found = true;
                }
            });
            return found;
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
        std::vector<Member> members_of(Context& c, const BitGroup& g, const char** why) {
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
            if (has_bit_order(c)) {
                return refuse("bit_order の指定がある");
            }
            std::vector<Member> ms;
            std::size_t used = 0;
            for (auto f : g.fields) {
                if (!is_layout_field(a, f)) {
                    return refuse("読み書きの対象でない field");
                }
                auto args = field_args(a, f);
                if (args.named || args.positional > 1) {
                    return refuse("field の引数");
                }
                if (g.bits > 8) {
                    auto* fe = c.tables.table<FieldEndian>().get(f);
                    if (fe && (fe->dynamic || fe->endian == Endian::little || fe->endian == Endian::native)) {
                        return refuse("2 バイト以上でビッグエンディアンでない");
                    }
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
                used += m.bits;
                ms.push_back(m);
            }
            if (used != g.bits) {
                return refuse("幅の表と型の幅が食い違う");
            }
            // 先頭の field が上位。
            std::size_t rest = g.bits;
            for (auto& m : ms) {
                rest -= m.bits;
                m.shift = rest;
            }
            return ms;
        }

        std::uint64_t mask_of(std::size_t bits) {
            return bits >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
        }

    }  // namespace

    const char* bit_group_refusal(Context& c, const BitGroup& g) {
        const char* why = nullptr;
        members_of(c, g, &why);
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
            if (is_transparent(s.type())) {
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

    Node<Body> lower_bit_group_decode(Context& c, const BitGroup& g, std::span<const Node<Expr>> targets,
                                      Node<Expr> bytes, Node<Expr> offset) {
        auto& a = c.a;
        if (targets.size() != g.fields.size()) {
            return nullref;
        }
        const char* why = nullptr;
        auto ms = members_of(c, g, &why);
        if (ms.empty()) {
            return nullref;
        }
        Builder b{a, g.fields.front().ref(a).loc()};
        auto raw = b.int_type(g.bits);
        auto whole = combine_int(c, bytes, offset, raw, Endian::big);
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
        auto ms = members_of(c, g, &why);
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
        auto body = split_int(c, bytes, offset, b.ref(def.ref(a)->name.ref(a)->identifier, raw), raw, Endian::big);
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

}  // namespace brgen::nast::lowering
