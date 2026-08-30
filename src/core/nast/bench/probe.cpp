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
#include "../lowering/int_bytes.hpp"
#include "../lowering/match_to_if.hpp"
#include "../lowering/predicate.hpp"
#include "../node/util.h"
#include "../parse/unparse.h"

#include <map>
#include <print>
#include <string_view>

using namespace brgen::nast;

namespace {

    using Hist = std::map<std::string, std::size_t>;

    template <class T, NodeType Kind>
    Node<T> node_at(std::uint32_t id) {
        return Node<T>::from_unique_id((std::uint64_t(Kind) << 32) | id);
    }

    // ---- size ------------------------------------------------------------

    void probe_size(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        auto& t = p.tables;
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (h && h->type == NodeType::ArrayType) {
                auto at = node_at<ArrayType, NodeType::ArrayType>(id);
                auto* es = t.table<TypeSize>().get(at.ref(a)->element_type);
                if (es && es->kind == SizeKind::dynamic) {
                    hist["  (配列: 要素が dynamic = 畳み込みが要る)"]++;
                }
            }
            if (!h || h->type != NodeType::Format) {
                continue;
            }
            auto f = node_at<Format, NodeType::Format>(id);
            auto* s = t.table<TypeSize>().get(f.ref(a)->struct_type);
            if (!s) {
                continue;
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
        }
    }

    // ---- endian ----------------------------------------------------------

    void probe_endian(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (!h || h->type != NodeType::Field) {
                continue;
            }
            auto f = node_at<Field, NodeType::Field>(id);
            auto* e = p.tables.table<FieldEndian>().get(f);
            if (!e) {
                continue;
            }
            std::string k = e->dynamic ? "dynamic" : to_string(e->endian);
            hist[k]++;
            if (detail) {
                auto name = name_of(a, f);
                std::println("{:<24} {}{}", name.empty() ? "(無名)" : std::string(name), k,
                             e->dynamic ? "  " + unparse_node(a, e->dynamic.ref(a)->order) : "");
            }
        }
        // 動的な順を含む format の数。呼び出しに乗せる規則にした場合の波及元。
        for (std::uint32_t id = 1; id <= a.node_count(); id++) {
            auto* h = a.header_at(id);
            if (!h || h->type != NodeType::Format) {
                continue;
            }
            auto f = node_at<Format, NodeType::Format>(id);
            auto* st = p.tables.table<FormatState>().get(f);
            if (!st) {
                continue;
            }
            for (auto& fld : st->fields) {
                auto* e = p.tables.table<FieldEndian>().get(fld);
                if (e && e->dynamic) {
                    hist["  (動的な順を含む format)"]++;
                    break;
                }
            }
        }
    }

    // ---- lower -----------------------------------------------------------

    Node<Expr> named_ref(Arena& a, brgen::lexer::Loc loc, std::string_view name, Node<Type> type) {
        auto r = a.make<Reference>(loc);
        r->name = a.make<Ident>(loc);
        r->name.ref(a)->identifier = std::string(name);
        r->type = type;
        return r;
    }

    void probe_lower_field(Program& p, lowering::Context& c, Node<Field> f, std::uint32_t id) {
        auto& a = p.arena;
        auto ty = f.ref(a)->type;
        if (!ty || !ty.as_any<IntType>()) {
            return;
        }
        auto loc = a.header_at(id)->loc;
        // バッファ名は呼ぶ側が決めるものなので仮に置く。
        auto buf = named_ref(a, loc, "buf", nullref);
        auto off = named_ref(a, loc, "o", nullref);
        auto target = a.make<Reference>(loc);
        target->name = f.ref(a)->name;
        target->type = ty;

        auto* fe = p.tables.table<FieldEndian>().get(f);
        auto order = fe && !fe->dynamic ? fe->endian : Endian::unspec;
        // 順が決まらない (native / 実行時) なら両方の形を出す。
        //
        // 選択子はここでは作れないので名前を置くだけにしている。実際に何を
        // 渡せるかは決まっていない: 実行時なら「代入の位置で 1 度評価した
        // 変数との比較」で nast の式として書けるが、**native の選択子は
        // ターゲット言語のテキスト** (cfg!(target_endian = "little") など) で、
        // nast の式にならない。EBM はノード (IS_LITTLE_ENDIAN) を残して綴りを
        // emit まで遅らせることでこれを成立させている。ここで普通の変数名を
        // 置くと動いているように見えてしまうので、埋める場所だと分かる名前に
        // してある。
        bool undecided = fe && (fe->dynamic || fe->endian == Endian::native);

        Node<Expr> combined;
        Node<Body> split;
        if (undecided) {
            auto sel = named_ref(a, loc, "SELECTOR_TO_BE_FILLED_BY_BACKEND",
                                 a.make<BoolType>(loc));
            combined = lowering::combine_int_either(c, buf, off, ty, sel);
            split = lowering::split_int_either(c, buf, off, target, ty, sel);
        }
        else {
            combined = lowering::combine_int(c, buf, off, ty, order);
            split = lowering::split_int(c, buf, off, target, ty, order);
        }
        if (!combined) {
            return;
        }
        std::println("--- {} :{}", name_of(a, f), unparse_node(a, ty));
        std::println("decode: {} = {}", name_of(a, f), unparse_node(a, combined));
        std::println("encode:");
        // unparse は Body 単体を綴らない (文ではない) ので 1 つずつ。
        if (split) {
            for (auto& s : split.ref(a)->statements) {
                std::println("{}", unparse_node(a, s));
            }
        }
    }

    void probe_lower(Program& p, bool detail, Hist& hist) {
        auto& a = p.arena;
        lowering::Context c{a, p.tables};
        auto last = a.node_count();  // 合成した分は歩かない
        for (std::uint32_t id = 1; id <= last; id++) {
            auto* h = a.header_at(id);
            if (!h) {
                continue;
            }
            if (h->type == NodeType::Match) {
                if (auto if_ = lowering::lower_match(c, node_at<Match, NodeType::Match>(id))) {
                    hist["match -> if"]++;
                    if (detail) {
                        std::println("--- match #{}", id);
                        std::println("{}", unparse_node(a, if_));
                    }
                }
                continue;
            }
            if (h->type == NodeType::Field) {
                if (detail) {
                    probe_lower_field(p, c, node_at<Field, NodeType::Field>(id), id);
                }
                continue;
            }
            if (h->type == NodeType::Binary) {
                auto b = node_at<Binary, NodeType::Binary>(id);
                if (auto e = lowering::lower_range_compare(c, b)) {
                    hist["範囲比較 -> 比較"]++;
                    if (detail) {
                        std::println("--- {}", unparse_node(a, b));
                        std::println("{}", unparse_node(a, e));
                    }
                }
                continue;
            }
            if (h->type == NodeType::Cond) {
                auto cond = node_at<Cond, NodeType::Cond>(id);
                auto* low = lowering::lower_conditional(c, cond);
                if (!low) {
                    hist["三項 (型が無く落とせない)"]++;
                    continue;
                }
                hist["三項 -> if"]++;
                if (detail) {
                    std::println("--- {}", unparse_node(a, cond));
                    std::println("{}", unparse_node(a, low->branch));
                    std::println("use: {}", unparse_node(a, low->value));
                }
            }
        }
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
        std::size_t total = 0;
        for (auto& [k, v] : hist) {
            if (!k.starts_with("  ")) {
                total += v;
            }
        }
        for (auto& [k, v] : hist) {
            std::println("{:<32} {:>6}{}", k, v,
                         !k.starts_with("  ") && total
                             ? std::format("  {:>5.1f}%", 100.0 * double(v) / double(total))
                             : "");
        }
        std::println("{:<32} {:>6}  ({} files)", "合計", total, files);
    }
}
