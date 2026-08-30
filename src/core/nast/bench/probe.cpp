// 解析と lowering の結果を印字して眺めるための入り口。
//
//   nast_probe size    <file.bgn>...   型の幅と幅の式
//   nast_probe endian  <file.bgn>...   field ごとに効いているバイト順
//   nast_probe lower   <file.bgn>...   三項 / match / 範囲比較 / 整数とバイト列
//
// ファイルが 1 つなら明細、2 つ以上なら集計。合成した木の正しさは、綴りに
// 戻して眺めるのが一番速い (docs/size_and_lowering.md 末尾)。
#include "../bind/pipeline.h"
#include "../lowering/conditional.hpp"
#include "../lowering/field_io.hpp"
#include "../lowering/int_bytes.hpp"
#include "../lowering/match_to_if.hpp"
#include "../lowering/predicate.hpp"
#include "../lowering/stream_io.hpp"
#include "../node/build.h"
#include "../node/util.h"
#include "../parse/unparse.h"

#include <map>
#include <print>
#include <string_view>

using namespace brgen::nast;

namespace {

    using Hist = std::map<std::string, std::size_t>;

    // ---- size ------------------------------------------------------------

    void probe_size(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        auto& t = p.tables;
        // 畳み込みが要る配列 (要素ごとに幅が違う) の数。
        each_node<ArrayType>(a, [&](Node<ArrayType> at) {
            auto* es = t.table<TypeSize>().get(at.ref(a)->element_type);
            if (es && es->kind == SizeKind::dynamic) {
                hist["  (配列: 要素が dynamic = 畳み込みが要る)"]++;
            }
        });
        each_node<Format>(a, [&](Node<Format> f) {
            auto* s = t.table<TypeSize>().get(f.ref(a)->struct_type);
            if (!s) {
                return;
            }
            const char* k = s->kind == SizeKind::fixed      ? "fixed"
                            : s->kind == SizeKind::dynamic  ? "dynamic"
                                                            : "unknown";
            hist[s->kind == SizeKind::dynamic ? (s->bits_expr ? "dynamic (式あり)" : "dynamic (式なし)") : k]++;
            if (detail) {
                std::string tail;
                if (s->kind == SizeKind::fixed) {
                    tail = std::format("  {} bits ({} bytes)", s->bits, s->bits / 8);
                }
                else if (s->bits_expr) {
                    tail = "  " + unparse_node(a, s->bits_expr) + " bits";
                }
                std::println("{:<28} {}{}", name_of(a, f), k, tail);
            }
        });
    }

    // ---- endian ----------------------------------------------------------

    void probe_endian(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        each_node<Field>(a, [&](Node<Field> f) {
            auto* e = p.tables.table<FieldEndian>().get(f);
            if (!e) {
                return;
            }
            std::string k = e->dynamic ? "dynamic" : to_string(e->endian);
            hist[k]++;
            if (detail) {
                auto name = name_of(a, f);
                std::println("{:<24} {}{}", name.empty() ? "(無名)" : std::string(name), k,
                             e->dynamic ? "  " + unparse_node(a, e->dynamic.ref(a)->order) : "");
            }
        });
        // 動的な順を含む format の数。呼び出しに乗せる規則にした場合の波及元。
        each_node<Format>(a, [&](Node<Format> f) {
            auto* st = p.tables.table<FormatState>().get(f);
            if (!st) {
                return;
            }
            for (auto& fld : st->fields) {
                auto* e = p.tables.table<FieldEndian>().get(fld);
                if (e && e->dynamic) {
                    hist["  (動的な順を含む format)"]++;
                    return;
                }
            }
        });
    }

    // ---- lower -----------------------------------------------------------

    void probe_lower_field(Program& p, lowering::Context& c, Node<Field> f, std::uint32_t id) {
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
        auto target = a.make<Reference>(loc);
        target->name = f.ref(a)->name;
        target->type = ty;

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
        if (!ty || name_of(a, f).empty()) {
            return;  // 無名 (分岐が合成したもの) は読み書きの対象ではない
        }
        auto loc = a.header_at(id)->loc;
        Builder b{a, loc};
        auto buf = b.ref("buf");
        auto off = b.ref("o");
        auto target = a.make<Reference>(loc);
        target->name = f.ref(a)->name;
        target->type = ty;
        if (lowering::lower_field_decode(c, f, target, buf, off)) {
            hist["field: 組めた"]++;
            return;
        }
        auto stripped = strip_wrappers(a, ty);
        auto kind = stripped ? to_string(stripped.type()) : "(型なし)";
        hist[std::format("field: 組めない ({})", kind)]++;
    }

    void probe_lower(Program& p, bool detail, Hist& hist) {
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
                probe_lower_field(p, c, f, f.id());
            }
            else {
                count_field(p, c, f, f.id(), hist);
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

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::println(stderr, "usage: nast_probe <size|endian|lower> <file.bgn>...");
        std::println(stderr, "  ファイルが 1 つなら明細、2 つ以上なら集計");
        return 2;
    }
    std::string_view what = argv[1];
    bool detail = argc == 3;
    Hist hist;
    std::size_t files = 0;
    for (int i = 2; i < argc; i++) {
        Program p;
        if (analyze(p, argv[i]) != AnalyzeResult::ok) {
            continue;
        }
        files++;
        if (what == "size") {
            probe_size(p, detail, hist);
        }
        else if (what == "endian") {
            probe_endian(p, detail, hist);
        }
        else if (what == "lower") {
            probe_lower(p, detail, hist);
        }
        else {
            std::println(stderr, "unknown mode: {}", what);
            return 2;
        }
    }
    if (!detail) {
        // 行は母集団ごとに分かれている (`field: ...` と lowering の規則)。
        // 混ぜて割ると割合が意味を失うので、`:` の手前を群として分けて数える。
        // 先頭が空白の行は内訳なので母数に入れない。
        std::map<std::string, std::size_t> group_total;
        auto group_of = [](const std::string& k) {
            auto pos = k.find(':');
            return pos == std::string::npos ? std::string() : k.substr(0, pos);
        };
        for (auto& [k, v] : hist) {
            if (!k.starts_with("  ")) {
                group_total[group_of(k)] += v;
            }
        }
        for (auto& [k, v] : hist) {
            auto total = k.starts_with("  ") ? 0 : group_total[group_of(k)];
            std::println("{:<32} {:>6}{}", k, v,
                         total ? std::format("  {:>5.1f}%", 100.0 * double(v) / double(total)) : "");
        }
        for (auto& [g, v] : group_total) {
            std::println("{:<32} {:>6}", g.empty() ? "合計" : g + " 合計", v);
        }
        std::println("({} files)", files);
    }
}
