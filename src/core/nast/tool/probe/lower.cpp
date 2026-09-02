/*license*/
// `nast_probe lower` — 三項 / match / 範囲比較 / 整数とバイト列。明細は綴りに
// 戻して出す (合成した木の正しさは .bgn として眺めるのが一番速い)。
#include "../../node/console.h"
#include "probe.hpp"
#include "../../query/lower_view.hpp"
#include "../../lowering/available.hpp"
#include "../../lowering/conditional.hpp"
#include "../../lowering/enum_defined.hpp"
#include "../../lowering/field_io.hpp"
#include "../../lowering/int_bytes.hpp"
#include "../../lowering/match_to_if.hpp"
#include "../../lowering/predicate.hpp"
#include "../../lowering/self_ref.hpp"
#include "../../lowering/stream_io.hpp"
#include "../../node/build.h"
#include "../../node/util.h"
#include "../../parse/unparse.h"


namespace brgen::nast::probe {

    namespace {

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
            // 引数の内訳。位置で書かれたものは assert に落ちるので組めた側に
            // 入るが、名前つきが付いた field は組めない。次に何を足すかの目安。
            auto args = field_args(a, f);
            if (args.positional) {
                hist["field 引数: 位置指定 (期待値)"]++;
                // 期待値は assert に落ちるが、落とせるのは読み書きが組める
                // field だけ。型で内訳を出す。
                auto at = strip_wrappers(a, ty);
                hist[std::format("  期待値の型: {}", at ? to_string(at.type()) : "(型なし)")]++;
            }
            if (args.named) {
                hist["field 引数: 名前つき"]++;
            }
            auto loc = a.header_at(id)->loc;
            Builder b{a, loc};
            auto buf = b.ref("buf");
            auto off = b.ref("o");
            auto target = lowering::field_ref(c, f);
            if (lowering::lower_field_decode(c, f, target, buf, off)) {
                hist["field: 組めた"]++;
                if (args.positional) {
                    hist["  うち期待値の assert つき"]++;
                }
                return;
            }
            if (args.named || args.positional > 1) {
                hist["field: 組めない (引数)"]++;
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
                    print_text("{}", query::lower_text(p, m));
                }
            }
        });
        each_node<Field>(a, [&](Node<Field> f) {
            if (detail) {
                print_text("{}", query::lower_text(p, f));
            }
            else {
                count_field(p, c, f, f.id(), hist);
            }
        });
        each_node<Available>(a, last, [&](Node<Available> av) {
            auto e = lowering::lower_available(c, av);
            if (!e) {
                hist["available (組めない)"]++;
                if (detail) {
                    print_text("{}", query::lower_text(p, av));
                }
                return;
            }
            hist["available -> 式"]++;
            if (detail) {
                print_text("{}", query::lower_text(p, av));
            }
        });
        each_node<MemberAccess>(a, last, [&](Node<MemberAccess> ma) {
            if (auto e = lowering::lower_enum_is_defined(c, ma)) {
                hist["is_defined -> 比較"]++;
                if (detail) {
                    print_text("{}", query::lower_text(p, ma));
                }
            }
        });
        each_node<Binary>(a, last, [&](Node<Binary> bin) {
            if (auto e = lowering::lower_range_compare(c, bin)) {
                hist["範囲比較 -> 比較"]++;
                if (detail) {
                    print_text("{}", query::lower_text(p, bin));
                }
            }
        });
        each_node<Cond>(a, last, [&](Node<Cond> cond) {
            auto* low = lowering::lower_conditional(c, cond);
            if (!low) {
                hist["三項 (型が無く落とせない)"]++;
                if (detail) {
                    print_text("{}", query::lower_text(p, cond));
                }
                return;
            }
            hist["三項 -> if"]++;
            if (detail) {
                print_text("{}", query::lower_text(p, cond));
            }
        });
    }

}  // namespace brgen::nast::probe
