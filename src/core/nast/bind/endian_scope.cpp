/*license*/
#include "endian_scope.hpp"
#include "../node/util.h"

namespace brgen::nast::bind {

    namespace {
        // `config.endian.little` のような綴りから、組み込みの列挙 (endian /
        // bit_order) のどのメンバかを出す。整数に畳んだ値を使わないのは、列挙の
        // 並び順に依存させないため。
        std::string_view builtin_member(Arena& a, SideTables& tables, Node<Expr> e, std::string_view enum_name) {
            Node<Ident> member;
            if (auto ma = e.as_any<MemberAccess>()) {
                member = ma.ref(a)->member;
            }
            else if (auto ref = e.as_any<Reference>()) {
                member = ref.ref(a)->name;
            }
            if (!member) {
                return {};
            }
            auto* res = tables.table<Resolution>().get(member);
            if (!res) {
                return {};
            }
            auto em = res->target.as_any<EnumMember>();
            if (!em) {
                return {};
            }
            auto belong = em.ref(a)->belong;
            if (!belong || name_of(a, belong) != enum_name) {
                return {};
            }
            return name_of(a, em);
        }

        std::optional<Endian> static_endian(Arena& a, SideTables& tables, Node<Expr> e) {
            auto text = builtin_member(a, tables, e, "endian");
            if (text == "big") {
                return Endian::big;
            }
            if (text == "little") {
                return Endian::little;
            }
            if (text == "native") {
                return Endian::native;
            }
            return std::nullopt;
        }

        std::optional<BitOrder> static_bit_order(Arena& a, SideTables& tables, Node<Expr> e) {
            auto text = builtin_member(a, tables, e, "bit_order");
            if (text == "msb") {
                return BitOrder::msb;
            }
            if (text == "lsb") {
                return BitOrder::lsb;
            }
            return std::nullopt;
        }

        // 型に綴りとして書かれたバイト順。配列なら要素まで降りる。
        Endian written_endian(Arena& a, Node<Type> t) {
            auto stripped = strip_arrays(a, strip_wrappers(a, t));
            if (auto i = stripped.as_any<IntType>()) {
                return i.ref(a)->endian;
            }
            if (auto f = stripped.as_any<FloatType>()) {
                return f.ref(a)->endian;
            }
            return Endian::unspec;
        }
    }  // namespace

    bool EndianScope::set_from(Node<SpecifyOrder> order) {
        auto d = order.ref(a);
        if (d->name == "input.bit_order") {
            // ビット順。stream / mapping を分けて指定する形 (input.bit_order.stream
            // など) はコーパスに無いので、両方を一度に指定するこの形だけを見る。
            if (auto b = static_bit_order(a, tables, strip_paren(a, d->order))) {
                current.bit_order = *b;
                current.bit_order_dynamic = nullref;
                return true;
            }
            current.bit_order = BitOrder::unspec;
            current.bit_order_dynamic = order;
            return true;
        }
        if (d->name != "input.endian") {
            return false;
        }
        if (auto e = static_endian(a, tables, strip_paren(a, d->order))) {
            current.endian = *e;
            current.dynamic = nullref;
            return true;
        }
        // 静的に決まらない = 実行時。代入そのものを指す (式ではない)。
        current.endian = Endian::unspec;
        current.dynamic = order;
        return true;
    }

    void EndianScope::apply(Node<Field> f) {
        auto written = written_endian(a, f.ref(a)->type);
        FieldEndian entry;
        if (written != Endian::unspec) {
            // 型に書いてあるほうが強い。
            entry.endian = written;
        }
        else if (current.dynamic) {
            entry.endian = Endian::unspec;
            entry.dynamic = current.dynamic;
            dynamic++;
        }
        else if (current.endian != Endian::unspec) {
            entry.endian = current.endian;
        }
        else {
            entry.endian = Endian::big;  // 言語の既定
        }
        entry.bit_order = current.bit_order;
        entry.bit_order_dynamic = current.bit_order_dynamic;
        tables.table<FieldEndian>().set(f, std::move(entry));
        analyzed++;
    }

    void EndianScope::walk_body(Node<Body> body) {
        if (!body) {
            return;
        }
        // block に入ったら退避する。中で書き換えても外へは漏れない。
        auto saved = current;
        for (auto& s : body.ref(a)->statements) {
            walk_statement(s);
        }
        current = saved;
    }

    void EndianScope::walk_statement(Node<Statement> s) {
        if (!s) {
            return;
        }
        if (auto order = s.as_any<SpecifyOrder>()) {
            set_from(order);
            return;
        }
        if (auto f = s.as_any<Field>()) {
            apply(f);
            return;
        }
        // 分岐は式の位置に出る (If / Match は ConditionalExpr)。分岐ごとに
        // block なので、それぞれ退避して入る。
        if (auto cond = s.as_any<ConditionalExpr>()) {
            for (auto& block : cond.ref(a)->blocks) {
                walk_body(block.ref(a)->body);
            }
            return;
        }
        if (auto loop = s.as_any<Loop>()) {
            walk_body(loop.ref(a)->body);
            return;
        }
        if (auto loop = s.as_any<RangeLoop>()) {
            walk_body(loop.ref(a)->body);
            return;
        }
        // 入れ子の宣言。字句としては内側なので外の指定を引き継ぐが、body は
        // 独立した block なので出るときに戻る。呼び出し経由では及ばない
        // (この歩き方は呼び出しを辿らない)。
        if (auto named = s.as_any<NamedBodyStatement>()) {
            walk_body(named.ref(a)->body);
            return;
        }
    }

    void EndianScope::run(const std::vector<Node<Module>>& modules) {
        for (auto& mod : modules) {
            // Module は block ではないので、指定はそこから先のファイル全体に
            // 効く。ここで退避しないのはそのため。
            current = State{};
            for (auto& s : mod.ref(a)->statements) {
                walk_statement(s);
            }
        }
    }

}  // namespace brgen::nast::bind
