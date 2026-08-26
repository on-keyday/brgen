/*license*/
#include "typer.hpp"

#include "../traverse.h"

#include <binary/log2i.h>

namespace brgen::nast::bind {

    namespace {

        // 収まる最小のバイト境界幅。元の ast::aligned_bit に当たる。
        std::size_t aligned_bit(std::size_t bit) {
            for (std::size_t n : {8, 16, 32, 64}) {
                if (bit <= n) {
                    return n;
                }
            }
            return 64;
        }

    }  // namespace

    Node<Type> Typer::struct_type_of(Node<Statement> owner) {
        auto st = owner.as_any<NamedStructTypedStatement>();
        if (!st) {
            return {};
        }
        auto* d = a.get<NamedStructTypedStatement>(st);
        if (!d->struct_type) {
            d->struct_type = a.make<StructType>(a.header_at(owner.id())->loc);
            a.get<StructType>(d->struct_type)->base = owner;
        }
        return d->struct_type;
    }

    Node<Type> Typer::struct_type_of_module(Node<Module> mod) {
        auto* d = a.get<Module>(mod);
        if (!d) {
            return {};
        }
        if (!d->struct_type) {
            d->struct_type = a.make<StructType>(a.header_at(mod.id())->loc);
            a.get<StructType>(d->struct_type)->base = mod;
        }
        return d->struct_type;
    }

    // 名前が指している宣言の型。Reference と IdentType の両方から使う。
    Node<Type> Typer::type_of_decl(Node<Statement> decl) {
        if (!decl) {
            return {};
        }
        // format / state。型としての同一性はここで作る StructType が持つ。
        if (decl.as_any<NamedStructTypedStatement>()) {
            return struct_type_of(decl);
        }
        if (auto en = decl.as_any<Enum>()) {
            return a.get<Enum>(en)->enum_type;
        }
        // field / parameter は宣言に書かれた型がそのまま。
        if (auto nt = decl.as_any<NamedTypeStatement>()) {
            return a.get<NamedTypeStatement>(nt)->type;
        }
        if (auto vd = decl.as_any<VariableDefinition>()) {
            return type_of_expr(a.get<VariableDefinition>(vd)->value);
        }
        if (auto fn = decl.as_any<Function>()) {
            auto* d = a.get<Function>(fn);
            auto ft = a.make<FunctionType>(a.header_at(decl.id())->loc);
            auto* ftd = a.get<FunctionType>(ft);
            ftd->return_type = d->return_type;
            for (auto& p : d->parameters) {
                ftd->parameters.push_back(a.get<Parameter>(p)->type);
            }
            return ft;
        }
        // EnumMember は所属する Enum への戻り参照を持たないので、ここからは
        // 型に辿り着けない。RangeLoop も束縛の型が container 側の要素型で、
        // どちらもこの段では出さない。
        return {};
    }

    // 型の包みを剥がして StructType を取り出す。IdentType は宣言を指しているだけ
    // なので base に降りる。
    Node<StructType> Typer::as_struct(Node<Type> t) {
        if (!t) {
            return {};
        }
        if (auto id = t.as_any<IdentType>()) {
            resolve_ident_type(id);
            return as_struct(a.get<IdentType>(id)->base);
        }
        if (auto st = t.as_any<StructType>()) {
            return st;
        }
        return {};
    }

    // struct の持ち主から名前でメンバを引く。
    //
    // format は binder が FormatState に集めたものを見る。分岐の中で宣言された
    // フィールドはそこで合成された union field になっていて、format 直下の
    // 一覧に並んでいる。木を歩くと分岐の中に埋もれていて引けない。
    // module (import 先) と state は表が無いので本体の文を順に見る。
    Node<Statement> Typer::lookup_member(Node<Statement> owner, std::string_view name) {
        auto named_name = [&](Node<Statement> s) -> std::string_view {
            if (auto n = s.as_any<NamedStatement>()) {
                if (auto* id = a.get<Ident>(a.get<NamedStatement>(n)->name)) {
                    return id->identifier;
                }
            }
            return {};
        };
        if (auto fmt = owner.as_any<Format>()) {
            if (auto* st = tables.table<FormatState>().get(fmt)) {
                for (auto& f : st->fields) {
                    if (named_name(f) == name) {
                        return f;
                    }
                }
                for (auto& f : st->functions) {
                    if (named_name(f) == name) {
                        return f;
                    }
                }
                for (auto& f : st->nested_formats) {
                    if (named_name(f) == name) {
                        return f;
                    }
                }
                for (auto& e : st->nested_enums) {
                    if (named_name(e) == name) {
                        return e;
                    }
                }
                return {};
            }
        }
        const std::vector<Node<Statement>>* statements = nullptr;
        if (auto mod = owner.as_any<Module>()) {
            statements = &a.get<Module>(mod)->statements;
        }
        else if (auto body = owner.as_any<NamedBodyStatement>()) {
            if (auto* b = a.get<Body>(a.get<NamedBodyStatement>(body)->body)) {
                statements = &b->statements;
            }
        }
        if (!statements) {
            return {};
        }
        for (auto& s : *statements) {
            if (named_name(s) == name) {
                return s;
            }
        }
        return {};
    }

    Node<Type> Typer::type_of_member_access(Node<MemberAccess> m) {
        auto* d = a.get<MemberAccess>(m);
        auto base_type = type_of_expr(d->base);
        if (!base_type) {
            return {};
        }
        auto* member = a.get<Ident>(d->member);
        if (!member) {
            return {};
        }
        auto loc = a.header_at(m.id())->loc;

        // enum の値。Color.red は Enum の members から引く。
        if (auto et = base_type.as_any<EnumType>()) {
            auto enum_ = a.get<EnumType>(et)->base;
            if (!enum_) {
                return {};
            }
            for (auto& mem : a.get<Enum>(enum_)->members) {
                if (a.get<Ident>(a.get<EnumMember>(mem)->name)->identifier == member->identifier) {
                    tables.table<Resolution>().set(d->member, Resolution{.target = mem});
                    return base_type;
                }
            }
            // enum の組み込みメンバ。値が定義済みかを聞く。
            if (member->identifier == "is_defined") {
                return a.make<BoolType>(loc);
            }
            return {};
        }
        // 配列の組み込みメンバ。要素数は使う側で決まるので幅は 64 固定。
        if (base_type.as_any<ArrayType>()) {
            if (member->identifier == "length") {
                auto t = a.make<IntType>(loc);
                auto* it = a.get<IntType>(t);
                it->bit_size = 64;
                it->is_signed = false;
                it->endian = Endian::unspec;
                return t;
            }
            return {};
        }
        auto st = as_struct(base_type);
        if (!st) {
            // UnionType (分岐で現れるフィールド) は共通型を出す段がまだ無い。
            return {};
        }
        auto owner = a.get<StructType>(st)->base;
        if (!owner) {
            return {};
        }
        auto found = lookup_member(owner, member->identifier);
        if (!found) {
            return {};
        }
        // メンバの名前も解決先を持たせておく。参照と同じ引き方ができる。
        tables.table<Resolution>().set(d->member, Resolution{.target = found});
        return type_of_decl(found);
    }

    Node<Type> Typer::type_of_expr(Node<Expr> e) {
        if (!e) {
            return {};
        }
        auto* base = a.get<Expr>(e);
        if (!base) {
            return {};
        }
        if (base->type) {
            return base->type;
        }
        // 相互参照で戻ってきたら諦める。x ::= y / y ::= x のような入力がある。
        if (!in_progress_.insert(e.id()).second) {
            return {};
        }
        auto loc = a.header_at(e.id())->loc;
        Node<Type> result;

        if (auto lit = e.as_any<IntLiteral>()) {
            auto t = a.make<IntLiteralType>(loc);
            a.get<IntLiteralType>(t)->base = lit;
            result = t;
        }
        else if (auto lit = e.as_any<StrLiteral>()) {
            auto t = a.make<StrLiteralType>(loc);
            a.get<StrLiteralType>(t)->base = lit;
            result = t;
        }
        else if (auto lit = e.as_any<RegexLiteral>()) {
            auto t = a.make<RegexLiteralType>(loc);
            a.get<RegexLiteralType>(t)->base = lit;
            result = t;
        }
        else if (e.as_any<BoolLiteral>()) {
            result = a.make<BoolType>(loc);
        }
        else if (auto ch = e.as_any<CharLiteral>()) {
            // 符号なしで、符号点が収まる最小のバイト境界幅。
            auto t = a.make<IntType>(loc);
            auto* d = a.get<IntType>(t);
            d->bit_size = aligned_bit(::futils::binary::log2i(a.get<CharLiteral>(ch)->code));
            d->is_signed = false;
            d->endian = Endian::unspec;
            result = t;
        }
        else if (e.as_any<TypeLiteral>()) {
            // 型そのものを値として書いたもの。u8 や format 名を式の位置に置いた形。
            result = a.make<MetaType>(loc);
        }
        else if (auto ref = e.as_any<Reference>()) {
            auto name = a.get<Reference>(ref)->name;
            if (auto* r = tables.table<Resolution>().get(name)) {
                result = type_of_decl(r->target);
            }
        }
        else if (auto paren = e.as_any<Paren>()) {
            result = type_of_expr(a.get<Paren>(paren)->expr);
        }
        else if (auto id = e.as_any<Identity>()) {
            result = type_of_expr(a.get<Identity>(id)->expr);
        }
        else if (auto ma = e.as_any<MemberAccess>()) {
            result = type_of_member_access(ma);
        }
        else if (auto import_ = e.as_any<Import>()) {
            // 読み込んだ Module の struct 型。メンバアクセスの左辺になる。
            if (auto* r = tables.table<ImportResolution>().get(import_)) {
                result = struct_type_of_module(r->module);
            }
        }

        in_progress_.erase(e.id());
        if (result) {
            a.get<Expr>(e)->type = result;
        }
        return result;
    }

    // 型の位置に書かれた名前。指している宣言の型を base に入れる。
    void Typer::resolve_ident_type(Node<IdentType> t) {
        auto* d = a.get<IdentType>(t);
        if (!d || d->base) {
            return;
        }
        auto* r = tables.table<Resolution>().get(d->ident);
        if (!r) {
            return;
        }
        d->base = type_of_decl(r->target);
    }

    void Typer::run(Node<Module> mod) {
        // 木の形に沿って一度舐める。足りないものは type_of_expr が
        // その場で解決先へ降りるので、順番には依存しない。
        std::vector<Node<Expr>> exprs;
        std::vector<Node<IdentType>> ident_types;
        visit_all(a, mod, [&](NodeAny n) {
            if (auto e = n.as_any<Expr>()) {
                exprs.push_back(e);
            }
            else if (auto it = n.as_any<IdentType>()) {
                ident_types.push_back(it);
            }
            return true;
        });
        // 型の名前を先に潰しておく。Reference の解決先が format のとき、
        // その StructType がここで作られていると参照が同じノードを指す。
        for (auto& it : ident_types) {
            resolve_ident_type(it);
        }
        for (auto& e : exprs) {
            if (type_of_expr(e)) {
                typed++;
            }
            else {
                untyped++;
            }
        }
    }

}  // namespace brgen::nast::bind
