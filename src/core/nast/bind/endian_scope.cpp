/*license*/
#include "endian_scope.hpp"
#include "../node/util.h"

namespace brgen::nast::bind {

    namespace {
        // `config.endian.little` のような綴りから値を出す。解決先が組み込みの
        // endian 列挙のメンバなら、その名前で決まる。整数に畳んだ値を使わない
        // のは、列挙の並び順に依存させないため。
        std::optional<Endian> static_endian(Arena& a, SideTables& tables, Node<Expr> e) {
            Node<Ident> member;
            if (auto ma = e.as_any<MemberAccess>()) {
                member = ma.ref(a)->member;
            }
            else if (auto ref = e.as_any<Reference>()) {
                member = ref.ref(a)->name;
            }
            if (!member) {
                return std::nullopt;
            }
            auto* res = tables.table<Resolution>().get(member);
            if (!res) {
                return std::nullopt;
            }
            auto em = res->target.as_any<EnumMember>();
            if (!em) {
                return std::nullopt;
            }
            auto belong = em.ref(a)->belong;
            if (!belong || name_of(a, belong) != "endian") {
                return std::nullopt;
            }
            auto text = name_of(a, em);
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
        if (d->name != "input.endian") {
            return false;  // bit_order 系はバイト順ではない
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
