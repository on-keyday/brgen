// 三項を文の形に落として印字する確認用。
#include "../bind/pipeline.h"
#include "../lowering/conditional.hpp"
#include "../lowering/match_to_if.hpp"
#include "../lowering/int_bytes.hpp"
#include "../lowering/predicate.hpp"
#include "../node/util.h"
#include "../parse/unparse.h"
#include <print>
using namespace brgen::nast;

int main(int argc, char** argv) {
    std::size_t n = 0;
    for (int i = 1; i < argc; i++) {
        Program p;
        if (analyze(p, argv[i]) != AnalyzeResult::ok) {
            continue;
        }
        auto& a = p.arena;
        lowering::Context c{a, p.tables};
        auto last = a.node_count();
        for (std::uint32_t id = 1; id <= last; id++) {
            auto* h = a.header_at(id);
            if (h && h->type == NodeType::Match) {
                auto m = Node<Match>::from_unique_id((std::uint64_t(NodeType::Match) << 32) | id);
                auto if_ = lowering::lower_match(c, m);
                if (if_ && argc == 2) {
                    std::println("--- match #{}", id);
                    std::println("{}", unparse_node(a, if_));
                }
                continue;
            }
            // field の整数を読み書きする形。バッファ名は呼ぶ側が決めるので
            // ここでは `buf` / `o` を仮に置く。
            if (h && h->type == NodeType::Field && argc == 2) {
                auto f = Node<Field>::from_unique_id((std::uint64_t(NodeType::Field) << 32) | id);
                auto ty = f.ref(a)->type;
                if (ty && ty.as_any<IntType>()) {
                    auto buf = a.make<Reference>(a.header_at(id)->loc);
                    buf->name = a.make<Ident>(a.header_at(id)->loc);
                    buf->name.ref(a)->identifier = "buf";
                    auto off = a.make<Reference>(a.header_at(id)->loc);
                    off->name = a.make<Ident>(a.header_at(id)->loc);
                    off->name.ref(a)->identifier = "o";
                    auto target = a.make<Reference>(a.header_at(id)->loc);
                    target->name = f.ref(a)->name;
                    target->type = ty;
                    auto combined = lowering::combine_int(c, buf, off, ty);
                    auto split = lowering::split_int(c, buf, off, target, ty);
                    if (combined) {
                        std::println("--- {} :{}", name_of(a, f), unparse_node(a, ty));
                        std::println("decode: {} = {}", name_of(a, f), unparse_node(a, combined));
                        std::println("encode:");
                        // unparse は Body 単体を綴らない (文ではない) ので 1 つずつ。
                        if (split) {
                            for (auto& s : split.ref(a)->statements) {
                                std::println("  {}", unparse_node(a, s));
                            }
                        }
                    }
                }
                continue;
            }
            if (h && h->type == NodeType::Binary) {
                auto b = Node<Binary>::from_unique_id((std::uint64_t(NodeType::Binary) << 32) | id);
                if (auto e = lowering::lower_range_compare(c, b); e && argc == 2) {
                    std::println("--- {}", unparse_node(a, b));
                    std::println("{}", unparse_node(a, e));
                }
                continue;
            }
            if (!h || h->type != NodeType::Cond) {
                continue;
            }
            auto cond = Node<Cond>::from_unique_id((std::uint64_t(NodeType::Cond) << 32) | id);
            auto* low = lowering::lower_conditional(c, cond);
            if (!low) {
                std::println("#{} 型が付いていないので落とせない", id);
                continue;
            }
            n++;
            if (argc == 2) {
                std::println("--- {}", unparse_node(a, cond));
                std::println("{}", unparse_node(a, low->branch));
                std::println("use: {}", unparse_node(a, low->value));
            }
        }
    }
    if (argc > 2) {
        std::println("{} 個の三項を落とした", n);
    }
}
