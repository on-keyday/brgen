/*license*/
// `nast_probe lower` — 三項 / match / 範囲比較 / 整数とバイト列。明細は綴りに
// 戻して出す (合成した木の正しさは .bgn として眺めるのが一番速い)。
#include "probe.hpp"
#include "../../lowering/available.hpp"
#include "../../lowering/conditional.hpp"
#include "../../lowering/field_io.hpp"
#include "../../lowering/int_bytes.hpp"
#include "../../lowering/match_to_if.hpp"
#include "../../lowering/predicate.hpp"
#include "../../lowering/self_ref.hpp"
#include "../../lowering/stream_io.hpp"
#include "../../node/build.h"
#include "../../node/util.h"
#include "../../parse/unparse.h"

#include <print>

namespace brgen::nast::probe {

    namespace {

        void print_field(Program& p, lowering::Context& c, Node<Field> f, std::uint32_t id) {
            auto& a = p.arena;
            auto ty = f.ref(a)->type;
            auto name = name_of(a, f);
            if (!ty || name.empty()) {
                return;
            }
            auto loc = a.header_at(id)->loc;
            // バッファと位置は呼ぶ側が決めるものなので仮に置く。
            Builder b{a, loc};
            auto buf = b.ref("buf");
            auto off = b.ref("o");
            auto target = lowering::field_ref(c, f);

            // 入力からバイトを並べるところ。int_bytes とは別の規則で、合成して
            // 初めて「読む」形になる。ここでは幅が固定の整数だけ見せる。
            Node<Expr> count;
            if (auto* s = p.tables.table<TypeSize>().get(strip_wrappers(a, ty));
                s && s->kind == SizeKind::fixed && s->bits % 8 == 0) {
                auto n = a.make<IntLiteral>(loc);
                n->value = std::to_string(s->bits / 8);
                count = n;
            }
            else if (auto arr = strip_wrappers(a, ty).as_any<ArrayType>()) {
                // 個数が式の配列。バイト数も式になるので、並べる側は回す形になる。
                auto* es = p.tables.table<TypeSize>().get(arr.ref(a)->element_type);
                if (es && es->kind == SizeKind::fixed && es->bits % 8 == 0 && arr.ref(a)->length &&
                    !arr.ref(a)->length.as_any<Range>()) {
                    auto w = a.make<IntLiteral>(loc);
                    w->value = std::to_string(es->bits / 8);
                    auto mul = a.make<Binary>(loc);
                    mul->op = BinaryOp::mul;
                    mul->left = arr.ref(a)->length;
                    mul->right = w;
                    count = mul;
                }
            }
            if (count) {
                auto fill = lowering::read_bytes(c, lowering::input_stream(c, loc), buf, nullref, count);
                if (fill) {
                    std::println("--- {} :{}", name, unparse_node(a, ty));
                    std::println("fill:");
                    for (auto& st : fill.ref(a)->statements) {
                        std::println("{}", unparse_node(a, st));
                    }
                    auto drain = lowering::write_bytes(c, lowering::output_stream(c, loc), buf, nullref, count);
                    std::println("drain:");
                    for (auto& st : drain.ref(a)->statements) {
                        std::println("{}", unparse_node(a, st));
                    }
                }
            }
            auto dec = lowering::lower_field_decode(c, f, target, buf, off);
            auto enc = lowering::lower_field_encode(c, f, target, buf, off);
            if (!dec) {
                return;
            }
            auto print_body = [&](const char* label, Node<Body> body) {
                std::println("{}:", label);
                if (!body) {
                    std::println("  (組めない)");
                    return;
                }
                // unparse は Body 単体を綴らない (文ではない) ので 1 つずつ。
                for (auto& s : body.ref(a)->statements) {
                    std::println("{}", unparse_node(a, s));
                }
            };
            print_body("decode", dec);
            print_body("encode", enc);
        }

        // 集計側。field の読み書きが組めるかどうかを数える。組めない理由は
        // 型で分けて出す (次に何を足すかの目安になる)。
        void count_field(Program& p, lowering::Context& c, Node<Field> f, std::uint32_t id, Hist& hist) {
            auto& a = p.arena;
            auto ty = f.ref(a)->type;
            if (!ty || name_of(a, f).empty() || !is_layout_field(a, f)) {
                // 無名の合成 field と、名前解決のための同名 field (UnionType) は
                // 読み書きの対象ではない。数えると分母が膨らむ。
                return;
            }
            auto loc = a.header_at(id)->loc;
            Builder b{a, loc};
            auto buf = b.ref("buf");
            auto off = b.ref("o");
            auto target = lowering::field_ref(c, f);
            if (lowering::lower_field_decode(c, f, target, buf, off)) {
                hist["field: 組めた"]++;
                return;
            }
            auto stripped = strip_wrappers(a, ty);
            auto kind = stripped ? to_string(stripped.type()) : "(型なし)";
            hist[std::format("field: 組めない ({})", kind)]++;
        }

    }  // namespace

    void run_lower(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        lowering::Context c{a, p.tables};
        // 走査を分けても、数えるのは元の木にあるものだけ。前の段が合成した
        // ノード (combine_int_either が作る三項など) を後の段が拾わないよう、
        // 範囲は始める前に固定して共有する。
        auto last = a.node_count();
        each_node<Match>(a, last, [&](Node<Match> m) {
            if (auto if_ = lowering::lower_match(c, m)) {
                hist["match -> if"]++;
                if (detail) {
                    std::println("--- match #{}", m.id());
                    std::println("{}", unparse_node(a, if_));
                }
            }
        });
        each_node<Field>(a, [&](Node<Field> f) {
            if (detail) {
                print_field(p, c, f, f.id());
            }
            else {
                count_field(p, c, f, f.id(), hist);
            }
        });
        each_node<Available>(a, last, [&](Node<Available> av) {
            auto e = lowering::lower_available(c, av);
            if (!e) {
                hist["available (組めない)"]++;
                return;
            }
            hist["available -> 式"]++;
            if (detail) {
                std::println("--- {}", unparse_node(a, av));
                std::println("{}", unparse_node(a, e));
            }
        });
        each_node<Binary>(a, last, [&](Node<Binary> bin) {
            if (auto e = lowering::lower_range_compare(c, bin)) {
                hist["範囲比較 -> 比較"]++;
                if (detail) {
                    std::println("--- {}", unparse_node(a, bin));
                    std::println("{}", unparse_node(a, e));
                }
            }
        });
        each_node<Cond>(a, last, [&](Node<Cond> cond) {
            auto* low = lowering::lower_conditional(c, cond);
            if (!low) {
                hist["三項 (型が無く落とせない)"]++;
                return;
            }
            hist["三項 -> if"]++;
            if (detail) {
                std::println("--- {}", unparse_node(a, cond));
                std::println("{}", unparse_node(a, low->branch));
                std::println("use: {}", unparse_node(a, low->value));
            }
        });
    }

}  // namespace brgen::nast::probe
