/*license*/
#include "lower_view.hpp"
#include "../lowering/available.hpp"
#include "../lowering/conditional.hpp"
#include "../lowering/field_io.hpp"
#include "../lowering/int_bytes.hpp"
#include "../lowering/match_to_if.hpp"
#include "../lowering/predicate.hpp"
#include "../lowering/self_ref.hpp"
#include "../lowering/stream_io.hpp"
#include "../node/build.h"
#include "../node/util.h"
#include "../parse/unparse.h"

#include <format>

namespace brgen::nast::query {

    namespace {

        void body_text(Arena& a, std::string& out, const char* label, Node<Body> body) {
            out += std::format("{}:\n", label);
            if (!body) {
                out += "  (組めない)\n";
                return;
            }
            // unparse は Body 単体を綴らない (文ではない) ので 1 つずつ。
            for (auto& s : body.ref(a)->statements) {
                out += unparse_node(a, s) + "\n";
            }
        }

        // 何バイト並べるか。幅が固定の整数と、要素幅が固定の配列だけ出せる。
        Node<Expr> byte_count(Program& p, Node<Type> ty, lexer::Loc loc) {
            auto& a = p.arena;
            auto stripped = strip_wrappers(a, ty);
            if (auto* s = p.tables.table<TypeSize>().get(stripped);
                s && s->kind == SizeKind::fixed && s->bits % 8 == 0) {
                auto n = a.make<IntLiteral>(loc);
                n->value = std::to_string(s->bits / 8);
                return n;
            }
            auto arr = stripped.as_any<ArrayType>();
            if (!arr) {
                return nullref;
            }
            auto* es = p.tables.table<TypeSize>().get(arr.ref(a)->element_type);
            if (!es || es->kind != SizeKind::fixed || es->bits % 8 != 0 || !arr.ref(a)->length ||
                arr.ref(a)->length.as_any<Range>()) {
                return nullref;
            }
            auto w = a.make<IntLiteral>(loc);
            w->value = std::to_string(es->bits / 8);
            auto mul = a.make<Binary>(loc);
            mul->op = BinaryOp::mul;
            mul->left = arr.ref(a)->length;
            mul->right = w;
            return mul;
        }

        std::string field_text(Program& p, lowering::Context& c, Node<Field> f) {
            auto& a = p.arena;
            auto ty = f.ref(a)->type;
            auto name = name_of(a, f);
            if (!ty || name.empty()) {
                return {};
            }
            auto loc = f.ref(a).loc();
            Builder b{a, loc};
            auto buf = b.ref("buf");
            auto off = b.ref("o");
            auto target = lowering::field_ref(c, f);

            std::string out = std::format("--- {} :{}\n", name, unparse_node(a, ty));
            if (auto count = byte_count(p, ty, loc)) {
                if (auto fill = lowering::read_bytes(c, lowering::input_stream(c, loc), buf, nullref, count)) {
                    body_text(a, out, "fill", fill);
                    body_text(a, out, "drain",
                              lowering::write_bytes(c, lowering::output_stream(c, loc), buf, nullref, count));
                }
            }
            auto dec = lowering::lower_field_decode(c, f, target, buf, off);
            if (!dec) {
                out += "decode:\n  (組めない)\n";
                return out;
            }
            body_text(a, out, "decode", dec);
            body_text(a, out, "encode", lowering::lower_field_encode(c, f, target, buf, off));
            return out;
        }

    }  // namespace

    std::string lower_text(Program& p, NodeAny n) {
        if (!n) {
            return {};
        }
        auto& a = p.arena;
        lowering::Context c{a, p.tables};
        if (auto f = n.as_any<Field>()) {
            return field_text(p, c, f);
        }
        auto head = std::format("--- {}\n", unparse_node(a, n));
        if (auto m = n.as_any<Match>()) {
            auto if_ = lowering::lower_match(c, m);
            return head + (if_ ? unparse_node(a, if_) + "\n" : "(組めない)\n");
        }
        if (auto av = n.as_any<Available>()) {
            auto e = lowering::lower_available(c, av);
            return head + (e ? unparse_node(a, e) + "\n" : "(組めない)\n");
        }
        if (auto cond = n.as_any<Cond>()) {
            auto* low = lowering::lower_conditional(c, cond);
            if (!low) {
                return head + "(組めない)\n";
            }
            return head + unparse_node(a, low->branch) + "\nuse: " + unparse_node(a, low->value) + "\n";
        }
        if (auto bin = n.as_any<Binary>()) {
            auto e = lowering::lower_range_compare(c, bin);
            if (!e) {
                return {};  // 範囲比較でない普通の二項は対象外
            }
            return head + unparse_node(a, e) + "\n";
        }
        return {};
    }

}  // namespace brgen::nast::query
