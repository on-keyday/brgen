/*license*/
#include "follow.hpp"
#include "../node/util.h"

#include <fnet/util/base64.h>

namespace brgen::nast::lowering {

    namespace {
        // 並びの解析から外す field。peek は読み進めない (元実装の
        // ignore_on_follow_analysis と同じ)。
        bool ignored(Arena& a, Node<Field> f) {
            if (!is_layout_field(a, f)) {
                return true;
            }
            auto args = f.ref(a)->arguments.ref(a);
            if (!args) {
                return false;
            }
            for (auto& arg : args->arguments) {
                auto na = arg.as_any<NamedArgument>();
                if (!na) {
                    continue;
                }
                auto name = na.ref(a)->name;
                if (auto m = name.as_any<MemberAccess>(); m && ident_text(a, m.ref(a)->member) == "peek") {
                    return true;
                }
            }
            return false;
        }

        std::optional<std::size_t> fixed_bits(Context& c, Node<Type> t) {
            auto* sz = t ? c.tables.table<TypeSize>().get(t) : nullptr;
            if (!sz || sz->kind != SizeKind::fixed) {
                return std::nullopt;
            }
            return sz->bits;
        }

        FieldFollow::Kind kind_of(Context& c, Node<Statement> s) {
            auto f = s.as_any<Field>();
            if (!f) {
                return FieldFollow::Kind::normal;  // 分岐
            }
            auto t = strip_wrappers(c.a, f.ref(c.a)->type);
            if (t.as_any<StrLiteralType>()) {
                return FieldFollow::Kind::constant;
            }
            return fixed_bits(c, f.ref(c.a)->type) ? FieldFollow::Kind::fixed : FieldFollow::Kind::normal;
        }
    }  // namespace

    FieldFollow field_follow(Context& c, Node<Body> body, std::size_t index) {
        auto& a = c.a;
        FieldFollow out;
        auto& stmts = body.ref(a)->statements;
        bool first = true;
        std::size_t tail = 0;
        for (auto i = index + 1; i < stmts.size(); i++) {
            auto s = stmts[i];
            if (auto f = s.as_any<Field>(); f && ignored(a, f)) {
                continue;
            }
            if (!s.as_any<Field>() && !s.as_any<ConditionalExpr>()) {
                continue;  // 定義 / assert / 宣言は読まない
            }
            auto k = kind_of(c, s);
            if (first) {
                out.kind = k;
                out.next = s.as_any<Field>();
                first = false;
            }
            if (k != FieldFollow::Kind::fixed) {
                out.eventual = k;
                return out;
            }
            tail += *fixed_bits(c, s.as_any<Field>().ref(a)->type);
        }
        out.eventual = FieldFollow::Kind::end;
        out.tail_bits = tail;
        return out;
    }

    std::optional<std::string> literal_bytes(Context& c, Node<Type> t) {
        auto s = strip_wrappers(c.a, t).as_any<StrLiteralType>();
        if (!s || !s.ref(c.a)->base) {
            return std::nullopt;
        }
        std::string decoded;
        if (!futils::base64::decode(s.ref(c.a)->base.ref(c.a)->binary_value, decoded)) {
            return std::nullopt;
        }
        return decoded;
    }

}  // namespace brgen::nast::lowering
