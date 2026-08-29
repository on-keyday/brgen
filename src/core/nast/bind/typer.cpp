/*license*/
#include "typer.hpp"

#include "../node/traverse.h"

#include "../node/compare.h"

#include <binary/log2i.h>
#include <number/parse.h>
#include <number/prefix.h>

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
            return nullref;
        }
        auto d = st.ref(a);
        if (!d->struct_type) {
            d->struct_type = a.make<StructType>(a.header_at(owner.id())->loc);
            d->struct_type.ref(a)->base = owner;
        }
        return d->struct_type;
    }

    Node<Type> Typer::struct_type_of_module(Node<Module> mod) {
        auto d = mod.ref(a);
        if (!d) {
            return nullref;
        }
        if (!d->struct_type) {
            d->struct_type = a.make<StructType>(a.header_at(mod.id())->loc);
            d->struct_type.ref(a)->base = mod;
        }
        return d->struct_type;
    }

    // 名前が指している宣言の型。Reference と IdentType の両方から使う。
    Node<Type> Typer::type_of_decl(Node<Statement> decl) {
        if (!decl) {
            return nullref;
        }
        // format / state。型としての同一性はここで作る StructType が持つ。
        if (decl.as_any<NamedStructTypedStatement>()) {
            return struct_type_of(decl);
        }
        if (auto en = decl.as_any<Enum>()) {
            return en.ref(a)->enum_type;
        }
        // field / parameter は宣言に書かれた型がそのまま。
        if (auto nt = decl.as_any<NamedTypeStatement>()) {
            return nt.ref(a)->type;
        }
        if (auto vd = decl.as_any<VariableDefinition>()) {
            auto d = vd.ref(a);
            if (d->op == BinaryOp::in_assign) {
                // for x in c の束縛。型は container 側から決まる。
                return iteration_type(type_of_expr(d->value));
            }
            return type_of_expr(d->value);
        }
        if (auto fn = decl.as_any<Function>()) {
            auto d = fn.ref(a);
            auto ft = a.make<FunctionType>(a.header_at(decl.id())->loc);
            auto ftd = ft;
            ftd->return_type = d->return_type;
            for (auto& p : d->parameters) {
                ftd->parameters.push_back(p.ref(a)->type);
            }
            return ft;
        }
        // enum 本体の中で他のメンバを裸の名前で参照した形。belong 経由で
        // 所属する Enum の型を出す。
        if (auto em = decl.as_any<EnumMember>()) {
            if (auto enum_ = em.ref(a)->belong) {
                return enum_.ref(a)->enum_type;
            }
            return nullref;
        }
        return nullref;
    }

    // for x in c で x に見える型。区分は EBM の変換 (rebrgen の
    // convert_loop_body: FOR_INT / FOR_RANGE / FOR_EACH) と揃えてある。
    //
    //   整数       その整数型 (リテラルは値が収まる幅の符号なし整数)
    //   文字列     u8
    //   範囲       両端の共通型。片端しか無ければその側から
    //   配列       要素型
    //
    // 反復を回すための counter 型や、開いた端の補完 (既定値 0 / 型の最大値) は
    // lowering 側の話で、束縛に見える型には出てこない。
    Node<Type> Typer::iteration_type(Node<Type> t) {
        if (!t) {
            return nullref;
        }
        if (auto id = t.as_any<IdentType>()) {
            resolve_ident_type(id);
            return iteration_type(id.ref(a)->base);
        }
        if (t.as_any<IntLiteralType>()) {
            // 相手なしの fit は、値が収まる最小のバイト境界幅に落ちる。
            return fit_int(t, t);
        }
        if (t.as_any<IntType>()) {
            return t;
        }
        if (t.as_any<StrLiteralType>()) {
            auto u8t = a.make<IntType>(a.header_at(t.id())->loc);
            auto d = u8t;
            d->bit_size = 8;
            d->is_signed = false;
            d->endian = Endian::unspec;
            return u8t;
        }
        if (auto r = t.as_any<RangeType>()) {
            // 基底は Range の型付けが両端から合成済み。リテラルなら上で幅に落ちる。
            return iteration_type(r.ref(a)->base_type);
        }
        if (auto arr = t.as_any<ArrayType>()) {
            return arr.ref(a)->element_type;
        }
        // それ以外は for in の対象として意味を決めていない。まだ締めない。
        return nullref;
    }

    // 型として同じか。equivalent は全フィールドの構造比較なので、式を抱える型で
    // 過剰に厳しくなる: ArrayType の length は別の式なら別ノードだし、その型
    // (RangeType) は weak の range 逆参照を id で比べる。元実装の equal_type は
    // 型ごとに「何が意味か」を選んでいた。その差分だけここで埋める。
    bool Typer::same_type(Node<Type> l, Node<Type> r) {
        if (!l || !r) {
            return false;
        }
        if (equivalent(a, l, r)) {
            return true;
        }
        if (auto la = l.as_any<ArrayType>()) {
            if (auto ra = r.as_any<ArrayType>()) {
                if (!same_type(la.ref(a)->element_type, ra.ref(a)->element_type)) {
                    return false;
                }
                // 長さは畳んだ定数で比べる。どちらも定数でなければ動的配列
                // どうしで等しい。片方だけ定数なら別の型 (元実装と同じ)。
                auto* lc = tables.table<ConstantValue>().get(la.ref(a)->length);
                auto* rc = tables.table<ConstantValue>().get(ra.ref(a)->length);
                if (lc && rc) {
                    return lc->kind == EvalKind::integer && rc->kind == EvalKind::integer &&
                           lc->integer == rc->integer && lc->is_negative == rc->is_negative;
                }
                return !lc && !rc;
            }
            return false;
        }
        if (auto lr = l.as_any<RangeType>()) {
            if (auto rr = r.as_any<RangeType>()) {
                auto lb = lr.ref(a)->base_type;
                auto rb = rr.ref(a)->base_type;
                if (!lb && !rb) {
                    return true;
                }
                return lb && rb && same_type(lb, rb);
            }
            return false;
        }
        // 文字列リテラル型は値で比べる。base は weak なので equivalent は
        // 別の出現をいつも別物にする。同じ magic ("\x00\x15") は同じ型。
        if (auto ls = l.as_any<StrLiteralType>()) {
            if (auto rs = r.as_any<StrLiteralType>()) {
                auto lv = ls.ref(a)->base.ref(a);
                auto rv = rs.ref(a)->base.ref(a);
                return lv && rv && lv->value == rv->value;
            }
            return false;
        }
        return false;
    }

    // 型の包みを剥がして StructType を取り出す。IdentType は宣言を指しているだけ
    // なので base に降りる。
    Node<StructType> Typer::as_struct(Node<Type> t) {
        if (!t) {
            return nullref;
        }
        if (auto id = t.as_any<IdentType>()) {
            resolve_ident_type(id);
            return as_struct(id.ref(a)->base);
        }
        if (auto imp = t.as_any<ImportedType>()) {
            // varint.Varint のような import 先の型。実体は import_ref の
            // メンバアクセスを式として解決すると出てくる struct 型。
            auto d = imp.ref(a);
            if (!d->base) {
                d->base = type_of_expr(d->import_ref);
            }
            return as_struct(d->base);
        }
        if (auto w = t.as_any<WrapperType>()) {
            // base を持つ包み全般。
            return as_struct(w.ref(a)->base);
        }
        if (auto ist = t.as_any<InlineStructType>()) {
            // 無名 format。持ち主の Format から struct 型を出して引く。
            return as_struct(struct_type_of(ist.ref(a)->inlined_format));
        }
        if (auto st = t.as_any<StructType>()) {
            return st;
        }
        return nullref;
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
                if (auto id = n.ref(a)->name.ref(a)) {
                    return id->identifier;
                }
            }
            return std::string_view{};  // 名前を持たない文
        };
        if (auto bound = owner.as_any<NamedBodyStatement>()) {
            if (auto* st = tables.table<FormatState>().get(bound)) {
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
                return nullref;
            }
        }
        const std::vector<Node<Statement>>* statements = nullptr;
        if (auto mod = owner.as_any<Module>()) {
            statements = &mod.ref(a)->statements;
        }
        else if (auto body = owner.as_any<NamedBodyStatement>()) {
            if (auto b = body.ref(a)->body.ref(a)) {
                statements = &b->statements;
            }
        }
        if (!statements) {
            return nullref;
        }
        for (auto& s : *statements) {
            if (named_name(s) == name) {
                return s;
            }
        }
        return nullref;
    }

    // メンバの型に現れる型パラメータを、その GenericType の型引数で置き換える。
    // 置き換えが要らなければ元のノードを返す。要る場合も置き換えの起きる包み
    // (ArrayType / GenericType) だけ新しく作り、中の式や具象型は共有する。
    // 元の generic format の宣言は書き換えない。
    Node<Type> Typer::substitute_type_params(Node<Type> t, const std::map<std::uint32_t, Node<Type>>& env) {
        if (!t) {
            return t;
        }
        if (auto id = t.as_any<IdentType>()) {
            if (auto* r = tables.table<Resolution>().get(id.ref(a)->ident)) {
                if (auto tp = r->target.as_any<TypeParameter>()) {
                    if (auto hit = env.find(tp.id()); hit != env.end()) {
                        return hit->second;
                    }
                }
            }
            return t;
        }
        if (auto arr = t.as_any<ArrayType>()) {
            auto elem = substitute_type_params(arr.ref(a)->element_type, env);
            if (elem == arr.ref(a)->element_type) {
                return t;
            }
            // 長さの式は共有する。長さは codec 意味論の情報で、型意味論では
            // 要素型だけが効く ([len]u8 の二重ビューと同じ扱い)。
            auto length = arr.ref(a)->length;
            auto end_loc = arr.ref(a)->end_loc;
            auto res = a.make<ArrayType>(a.header_at(t.id())->loc);
            auto rd = res;
            rd->length = length;
            rd->end_loc = end_loc;
            rd->element_type = elem;
            return res;
        }
        if (auto gt = t.as_any<GenericType>()) {
            // Bar の a :Foo[T] のような入れ子。引数側だけ置き換える。
            bool changed = false;
            std::vector<Node<Type>> args;
            for (auto& arg : gt.ref(a)->type_arguments) {
                auto s = substitute_type_params(arg, env);
                changed = changed || s != arg;
                args.push_back(s);
            }
            if (!changed) {
                return t;
            }
            auto base = gt.ref(a)->base_type;
            auto res = a.make<GenericType>(a.header_at(t.id())->loc);
            auto rd = res;
            rd->base_type = base;
            rd->type_arguments = std::move(args);
            return res;
        }
        return t;
    }

    Node<Type> Typer::type_of_member_access(Node<MemberAccess> m) {
        auto d = m.ref(a);
        auto base_type = type_of_expr(d->base);
        if (!base_type) {
            return nullref;
        }
        auto member = d->member.ref(a);
        if (!member) {
            return nullref;
        }
        auto loc = a.header_at(m.id())->loc;

        // 名前の包み (IdentType) を剥がしてから種別を見る。enum を別名や
        // フィールド型経由で参照した形 (x.is_defined など) がここで揃う。
        while (auto id = base_type.as_any<IdentType>()) {
            resolve_ident_type(id);
            auto b = id.ref(a)->base;
            if (!b) {
                break;
            }
            base_type = b;
        }

        // enum の値。Color.red は Enum の members から引く。
        if (auto et = base_type.as_any<EnumType>()) {
            auto enum_ = et.ref(a)->base;
            if (!enum_) {
                return nullref;
            }
            for (auto& mem : enum_.ref(a)->members) {
                if (mem.ref(a)->name.ref(a)->identifier == member->identifier) {
                    tables.table<Resolution>().set(d->member, Resolution{.target = mem});
                    return base_type;
                }
            }
            // enum の組み込みメンバ。値が定義済みかを聞く。
            if (member->identifier == "is_defined") {
                return a.make<BoolType>(loc);
            }
            return nullref;
        }
        // 配列の組み込みメンバ。要素数は使う側で決まるので幅は 64 固定。
        if (base_type.as_any<ArrayType>()) {
            if (member->identifier == "length") {
                auto t = a.make<IntType>(loc);
                auto it = t;
                it->bit_size = 64;
                it->is_signed = false;
                it->endian = Endian::unspec;
                return t;
            }
            return nullref;
        }
        // ストリームの組み込みメンバ。位置と残量は u64。呼び出し形 (get / peek /
        // subrange / backward / put) の戻り値は引数で決まるので Call 側で付ける。
        // 未知のメンバ (input.endian への代入など) はエラーにせず型なしのまま。
        // 検査で締めるのは使い方が見えてからにする。
        if (auto stream = base_type.as_any<StreamType>()) {
            if (stream.ref(a)->kind == SpecialLiteralKind::input_ &&
                (member->identifier == "offset" || member->identifier == "bit_offset" ||
                 member->identifier == "remain" || member->identifier == "scope_length")) {
                auto t = a.make<IntType>(loc);
                auto it = t;
                it->bit_size = 64;
                it->is_signed = false;
                it->endian = Endian::unspec;
                return t;
            }
            return nullref;
        }
        // 分岐で現れる同名フィールド (union) 越しのアクセスは共通型で引く。
        // 元実装の typing_member_access の lookup_union と同じ。
        if (auto u = base_type.as_any<UnionType>()) {
            resolve_union_type(u);
            if (auto ct = u.ref(a)->common_type) {
                base_type = ct;
            }
        }
        // generic format のメンバ。Foo[Plain] の x :T は Plain として見える。
        // 実体化 (monomorphize) の段は作らず、メンバを引くこの場で型引数を
        // 代入する。stream の型付けと同じ「置き換えの段を作らない」方針。
        if (auto gt = base_type.as_any<GenericType>()) {
            auto gd = gt.ref(a);
            auto base_ident = gd->base_type.as_any<IdentType>();
            if (!base_ident) {
                return nullref;
            }
            auto* r = tables.table<Resolution>().get(base_ident.ref(a)->ident);
            if (!r) {
                return nullref;
            }
            auto gf = r->target.as_any<GenericFormat>();
            if (!gf) {
                return nullref;
            }
            auto gfd = gf.ref(a);
            if (gfd->type_parameters.size() != gd->type_arguments.size()) {
                return nullref;  // 引数の数が合わない。検査で締めるのはまだ
            }
            std::map<std::uint32_t, Node<Type>> env;
            for (std::size_t i = 0; i < gfd->type_parameters.size(); i++) {
                env[gfd->type_parameters[i].id()] = gd->type_arguments[i];
            }
            auto found = lookup_member(gf, member->identifier);
            if (!found) {
                return nullref;
            }
            tables.table<Resolution>().set(d->member, Resolution{.target = found});
            return substitute_type_params(type_of_decl(found), env);
        }
        auto st = as_struct(base_type);
        if (!st) {
            return nullref;
        }
        auto owner = st.ref(a)->base;
        if (!owner) {
            return nullref;
        }
        auto found = lookup_member(owner, member->identifier);
        if (!found) {
            return nullref;
        }
        // メンバの名前も解決先を持たせておく。参照と同じ引き方ができる。
        tables.table<Resolution>().set(d->member, Resolution{.target = found});
        return type_of_decl(found);
    }

    // 整数リテラルの型を、相手が整数型ならその幅に合わせる。
    // 例: `x :u8` に対する `x == 3` の 3 は u8 として扱う。
    // 元の int_type_fitting は型ノードを書き換えるが、こちらは結果を返すだけ。
    // 書き換えると同じリテラル型を共有している他の式にも波及する。
    Node<Type> Typer::fit_int(Node<Type> t, Node<Type> other) {
        auto lit = t.as_any<IntLiteralType>();
        if (!lit) {
            return t;
        }
        if (auto it = other.as_any<IntType>()) {
            return other;
        }
        // 相手もリテラルなら、値が収まる最小のバイト境界幅の符号なし整数にする。
        // prefix_integer は 0x / 0b / 0o の前置も読む。
        auto d = lit.ref(a);
        std::size_t value = 0;
        auto raw = d->base.ref(a);
        if (!raw || !::futils::number::prefix_integer(raw->value, value)) {
            return t;
        }
        auto res = a.make<IntType>(a.header_at(t.id())->loc);
        auto rd = res;
        rd->bit_size = aligned_bit(::futils::binary::log2i(value));
        rd->is_signed = false;
        rd->endian = Endian::unspec;
        return res;
    }

    Node<Type> Typer::common_type(Node<Type> l, Node<Type> r) {
        if (!l || !r) {
            return nullref;
        }
        // fit は両方とも元の型に対して評価する。先に片方を代入すると、
        // リテラル同士のとき後の fit が具体化済みの相手に吸われて、
        // 常に左の値の幅になってしまう (0..300 が u8 になる)。
        auto fl = fit_int(l, r);
        auto fr = fit_int(r, l);
        l = fl;
        r = fr;
        if (equivalent(a, l, r)) {
            return l;
        }
        // 範囲どうしは基底の型だけで比べる。上下限は型の同一性に関与しない
        // (元実装 equal_type の RangeType 分岐)。equivalent は weak の range
        // 逆参照を id で比べるため、ここで受けないと別の範囲式から作られた
        // 範囲型どうしが常に不一致になる (match の 0xE1..=0xEC, 0xEE..=0xEF)。
        if (auto lr = l.as_any<RangeType>()) {
            if (auto rr = r.as_any<RangeType>()) {
                auto lb = lr.ref(a)->base_type;
                auto rb = rr.ref(a)->base_type;
                if (!lb && !rb) {
                    return l;
                }
                if (equivalent(a, fit_int(lb, rb), fit_int(rb, lb))) {
                    return l;
                }
                return nullref;
            }
        }
        // 配列。長さまで同じならそのまま、要素が同じなら長さを外して動的配列に
        // 畳む (元実装 tool::common_type の ArrayType 分岐と同じ)。
        if (auto la = l.as_any<ArrayType>()) {
            if (auto ra = r.as_any<ArrayType>()) {
                if (same_type(l, r)) {
                    return l;
                }
                auto elem = la.ref(a)->element_type;
                if (same_type(elem, ra.ref(a)->element_type)) {
                    auto res = a.make<ArrayType>(a.header_at(l.id())->loc);
                    res->element_type = elem;
                    return res;
                }
                return nullref;
            }
        }
        auto li = l.as_any<IntType>();
        auto ri = r.as_any<IntType>();
        if (li && ri) {
            auto ld = li.ref(a);
            auto rd = ri.ref(a);
            if (ld->bit_size == rd->bit_size) {
                // 幅が同じなら符号なしのほうに寄せる。符号付きを混ぜると
                // 表現できない値が出る。
                if (ld->is_signed == rd->is_signed) {
                    return l;
                }
                return ld->is_signed ? r : l;
            }
            return ld->bit_size > rd->bit_size ? l : r;
        }
        // 分岐の union は共通型に剥がして比べる。元実装の tool::common_type と同じ。
        if (auto u = l.as_any<UnionType>()) {
            resolve_union_type(u);
            if (auto ct = u.ref(a)->common_type) {
                return common_type(ct, r);
            }
            return nullref;
        }
        if (auto u = r.as_any<UnionType>()) {
            resolve_union_type(u);
            if (auto ct = u.ref(a)->common_type) {
                return common_type(l, ct);
            }
            return nullref;
        }
        // format の cast_fn 経由はまだ無い。
        return nullref;
    }

    // 分岐ごとに宣言された同名 field の共通型。元実装の typing_union_type と同じで、
    // 名前が現れない分岐の pad (field 無し) は飛ばし、入れ子の union は先に自分の
    // 共通型に潰してから畳む。is_strict は全候補が同じ型ノードに畳めたかどうか。
    void Typer::resolve_union_type(Node<UnionType> u) {
        auto d = u.ref(a);
        if (d->common_type) {
            return;
        }
        bool is_strict = false;
        Node<Type> common;
        for (auto& c : d->candidates) {
            auto f = c.ref(a)->field;
            if (!f) {
                continue;
            }
            auto ft = f.ref(a)->type;
            if (!common) {
                if (auto nested = ft.as_any<UnionType>()) {
                    resolve_union_type(nested);
                    common = nested.ref(a)->common_type;
                    is_strict = nested.ref(a)->is_strict_common_type;
                }
                else {
                    common = ft;
                    is_strict = true;
                }
            }
            else {
                auto before = common;
                common = common_type(common, ft);
                if (!common) {
                    is_strict = false;
                    break;
                }
                is_strict = is_strict && before == common;
            }
        }
        d->common_type = common;
        d->is_strict_common_type = is_strict;
    }

    Node<Type> Typer::type_of_binary(Node<Binary> b) {
        auto d = b.ref(a);
        auto loc = a.header_at(b.id())->loc;
        auto lty = type_of_expr(d->left);
        auto rty = type_of_expr(d->right);
        switch (d->op) {
            // 代入は式としては void。元実装の typing_assign と同じで、
            // 左辺が付かない代入 (input.endian = .. など) でも void は付く。
            case BinaryOp::assign:
            case BinaryOp::append_assign:
            case BinaryOp::add_assign:
            case BinaryOp::sub_assign:
            case BinaryOp::mul_assign:
            case BinaryOp::div_assign:
            case BinaryOp::mod_assign:
            case BinaryOp::left_logical_shift_assign:
            case BinaryOp::right_logical_shift_assign:
            case BinaryOp::left_arithmetic_shift_assign:
            case BinaryOp::right_arithmetic_shift_assign:
            case BinaryOp::bit_and_assign:
            case BinaryOp::bit_or_assign:
            case BinaryOp::bit_xor_assign:
                return a.make<VoidType>(loc);
            // 比較。両辺が付いていることだけ確かめる。比較可能かの検査はまだ。
            case BinaryOp::equal:
            case BinaryOp::not_equal:
            case BinaryOp::less:
            case BinaryOp::less_or_eq:
            case BinaryOp::grater:
            case BinaryOp::grater_or_eq:
                return (lty && rty) ? a.make<BoolType>(loc) : nullref;
            case BinaryOp::logical_and:
            case BinaryOp::logical_or:
                if (lty.as_any<BoolType>() && rty.as_any<BoolType>()) {
                    return a.make<BoolType>(loc);
                }
                return nullref;
            case BinaryOp::add:
            case BinaryOp::sub:
            case BinaryOp::mul:
            case BinaryOp::div:
            case BinaryOp::mod:
            case BinaryOp::left_logical_shift:
            case BinaryOp::right_logical_shift:
            case BinaryOp::left_arithmetic_shift:
            case BinaryOp::right_arithmetic_shift:
                return common_type(lty, rty);
            // ビット演算は幅を揃えることを求める。揃っていなければ型を出さない。
            case BinaryOp::bit_and:
            case BinaryOp::bit_or:
            case BinaryOp::bit_xor: {
                if (!lty || !rty) {
                    return nullref;
                }
                auto l = fit_int(lty, rty);
                auto r = fit_int(rty, lty);
                if (l.as_any<IntType>() && r.as_any<IntType>() && equivalent(a, l, r)) {
                    return l;
                }
                return nullref;
            }
            case BinaryOp::comma:
                return rty;
            default:
                // define_assign / const_assign / in_assign は文 (VariableDefinition /
                // RangeLoop) になっていてここには来ない。範囲は Range ノード。
                return nullref;
        }
    }

    // ブロックの値。最後の文が式ならその型で、そうでなければ無い。
    Node<Type> Typer::block_value_type(Node<Body> body) {
        auto d = body.ref(a);
        if (!d || d->statements.empty()) {
            return nullref;
        }
        auto last = d->statements.back().as_any<Expr>();
        if (!last) {
            return nullref;
        }
        return type_of_expr(last);
    }

    // if / match。分岐の値の型が揃えばその型、揃わなければ void。
    // .bgn では大半が文なので void になる。
    Node<Type> Typer::type_of_conditional(Node<ConditionalExpr> c) {
        auto d = c.ref(a);
        auto loc = a.header_at(c.id())->loc;
        if (auto m = c.as_any<Match>()) {
            type_of_expr(m.ref(a)->condition);
        }
        Node<Type> common;
        for (auto& block : d->blocks) {
            auto b = block.ref(a);
            if (auto cs = block.as_any<ConditionalStatement>()) {
                type_of_expr(cs.ref(a)->condition);
            }
            auto t = block_value_type(b->body);
            if (!t) {
                return a.make<VoidType>(loc);
            }
            if (!common) {
                common = t;
                continue;
            }
            // 元実装の typing_if / typing_match と同じで、リテラルは相手に
            // 寄せてから比べる (int_type_fitting -> equal_type)。
            auto fc = fit_int(common, t);
            auto ft = fit_int(t, common);
            if (!equivalent(a, fc, ft)) {
                return a.make<VoidType>(loc);
            }
            common = fc;
        }
        return common ? common : a.make<VoidType>(loc);
    }

    // ストリームの組み込みメソッドの呼び出し。該当しなければ型を出さず、
    // 呼び出し側で普通の関数呼び出しとして扱われる。
    //
    //   input.get(T) / input.peek(T)   引数 0 の型リテラルが名指す型。引数なしは u8
    //   input.subrange(len)            長さ len を確立した StreamType
    //   input.backward(..) / output.put(..)   void
    //
    // 元実装は resolve_io_operation がノードを IOOperation に置き換えてから
    // typing_io_operation (middle/typing.cpp:1381) が型を付けていた。nast は
    // 置き換えず、この場で型だけ付ける。引数の個数・種類の検査はまだしない。
    Node<Type> Typer::type_of_stream_call(Node<Call> call) {
        auto d = call.ref(a);
        auto ma = d->callee.as_any<MemberAccess>();
        if (!ma) {
            return nullref;
        }
        auto mad = ma.ref(a);
        auto stream = type_of_expr(mad->base).as_any<StreamType>();
        if (!stream) {
            return nullref;
        }
        auto member = mad->member.ref(a);
        if (!member) {
            return nullref;
        }
        auto kind = stream.ref(a)->kind;
        auto loc = a.header_at(call.id())->loc;
        Node<Expr> arg0;
        if (auto args = d->arguments.ref(a); args && !args->arguments.empty()) {
            arg0 = args->arguments.front().ref(a)->value;
        }
        Node<Type> result;
        if (kind == SpecialLiteralKind::input_ && (member->identifier == "get" || member->identifier == "peek")) {
            if (!arg0) {
                // 引数なしは u8。元実装の既定と同じ。
                auto t = a.make<IntType>(loc);
                auto it = t;
                it->bit_size = 8;
                it->is_signed = false;
                it->endian = Endian::unspec;
                result = t;
            }
            else if (auto lit = arg0.as_any<TypeLiteral>()) {
                result = lit.ref(a)->literal;
            }
            else if (auto ref = arg0.as_any<Reference>()) {
                // input.get(Lz4DataBlock) のように型を名前で渡す形。TypeLiteral に
                // parse されるのは u8 などの組み込み型だけで、format / enum の名前は
                // Reference で来る。解決先が型の宣言ならその型。
                if (auto* r = tables.table<Resolution>().get(ref.ref(a)->name)) {
                    if (r->target.as_any<NamedStructTypedStatement>() || r->target.as_any<Enum>()) {
                        result = type_of_decl(r->target);
                    }
                }
            }
        }
        else if (kind == SpecialLiteralKind::input_ && member->identifier == "subrange") {
            if (arg0) {
                // 長さの式は保持するだけで、型互換性の判定には使わない。
                // [len]u8 の length と同じ扱い (codec 意味論と型意味論の分離)。
                auto t = a.make<StreamType>(loc);
                auto st = t;
                st->kind = kind;
                st->length = arg0;
                result = t;
            }
        }
        else if ((kind == SpecialLiteralKind::input_ && member->identifier == "backward") ||
                 (kind == SpecialLiteralKind::output_ && member->identifier == "put")) {
            result = a.make<VoidType>(loc);
        }
        if (result) {
            // callee (input.get というメンバアクセス自体) にも関数型を付けておく。
            // 対応する宣言は無いので parameters は空のまま。
            if (!ma.ref(a)->type) {
                auto ft = a.make<FunctionType>(loc);
                ft->return_type = result;
                ma.ref(a)->type = ft;
            }
        }
        return result;
    }

    Node<Type> Typer::type_of_expr(Node<Expr> e) {
        if (!e) {
            return nullref;
        }
        auto base = e.ref(a);
        if (!base) {
            return nullref;
        }
        if (base->type) {
            return base->type;
        }
        // 相互参照で戻ってきたら諦める。x ::= y / y ::= x のような入力がある。
        if (!in_progress_.insert(e.id()).second) {
            return nullref;
        }
        auto loc = a.header_at(e.id())->loc;
        Node<Type> result;

        if (auto lit = e.as_any<IntLiteral>()) {
            auto t = a.make<IntLiteralType>(loc);
            t->base = lit;
            result = t;
        }
        else if (auto lit = e.as_any<StrLiteral>()) {
            auto t = a.make<StrLiteralType>(loc);
            t->base = lit;
            result = t;
        }
        else if (auto lit = e.as_any<RegexLiteral>()) {
            auto t = a.make<RegexLiteralType>(loc);
            t->base = lit;
            result = t;
        }
        else if (e.as_any<BoolLiteral>()) {
            result = a.make<BoolType>(loc);
        }
        else if (auto ch = e.as_any<CharLiteral>()) {
            // 符号なしで、符号点が収まる最小のバイト境界幅。
            auto t = a.make<IntType>(loc);
            auto d = t;
            d->bit_size = aligned_bit(::futils::binary::log2i(ch.ref(a)->code));
            d->is_signed = false;
            d->endian = Endian::unspec;
            result = t;
        }
        else if (e.as_any<TypeLiteral>()) {
            // 型そのものを値として書いたもの。u8 や format 名を式の位置に置いた形。
            result = a.make<MetaType>(loc);
        }
        else if (auto sp = e.as_any<SpecialLiteral>()) {
            // input / output はストリーム。どの実体 (全入力 / 逐次 / ビット列 ...) で
            // 呼ばれるかは format の側からは決まらないので、型にはプログラム自身が
            // 確立した性質だけを載せる。素の input は length の無い StreamType。
            auto kind = sp.ref(a)->kind;
            if (kind == SpecialLiteralKind::input_ || kind == SpecialLiteralKind::output_) {
                auto t = a.make<StreamType>(loc);
                t->kind = kind;
                result = t;
            }
            else if (kind == SpecialLiteralKind::config_) {
                // config は組み込み宣言のモジュール。config.endian.big は
                // Color.red と同じ enum メンバアクセスとして流れる。
                // config.url のような自由メタデータは宣言に無い名前なので
                // 解決されず型なしのまま (従来どおり別扱い)。
                result = struct_type_of_module(builtin_module());
            }
        }
        else if (auto ref = e.as_any<Reference>()) {
            auto name = ref.ref(a)->name;
            if (auto* r = tables.table<Resolution>().get(name)) {
                if (r->target.as_any<TypeParameter>()) {
                    // sizeof(T) など、型パラメータを式の位置に置いた形。指すのは
                    // 型そのもの (元実装は ident_to_type_literal で TypeLiteral に
                    // 書き換えて MetaType を付ける。nast は書き換えず型だけ揃える)。
                    result = a.make<MetaType>(loc);
                }
                else {
                    result = type_of_decl(r->target);
                }
            }
        }
        else if (auto paren = e.as_any<Paren>()) {
            result = type_of_expr(paren.ref(a)->expr);
        }
        else if (auto id = e.as_any<Identity>()) {
            result = type_of_expr(id.ref(a)->expr);
        }
        else if (auto bin = e.as_any<Binary>()) {
            result = type_of_binary(bin);
        }
        else if (auto un = e.as_any<Unary>()) {
            // - も ! も型を変えない。
            result = type_of_expr(un.ref(a)->target);
        }
        else if (auto idx = e.as_any<Index>()) {
            // 配列の要素型。添字の型は結果に効かない。
            auto d = idx.ref(a);
            type_of_expr(d->index);
            if (auto arr = type_of_expr(d->base).as_any<ArrayType>()) {
                result = arr.ref(a)->element_type;
            }
        }
        else if (auto rng = e.as_any<Range>()) {
            // 端が片方しか無い形 (`..x` / `x..`) もある。基底は両端の共通型。
            // 元実装の int_type_fitting と同じで、片側リテラルは他方の型に、
            // 両方リテラルは大きい方の値が収まる幅に寄る (0..300 は u16)。
            // 元実装は揃わなければエラーにしたが、ここではまだ締めず、
            // 基底なしにするだけ。
            auto d = rng.ref(a);
            auto st = type_of_expr(d->start);
            auto en = type_of_expr(d->end);
            auto t = a.make<RangeType>(loc);
            auto rd = t;
            rd->base_type = (st && en) ? common_type(st, en) : (st ? st : en);
            rd->range = rng;
            result = t;
        }
        else if (auto cast = e.as_any<Cast>()) {
            // 変換先は呼ばれている型リテラル。
            auto d = cast.ref(a);
            auto callee = d->base.ref(a)->callee;
            if (auto lit = callee.as_any<TypeLiteral>()) {
                result = lit.ref(a)->literal;
            }
            // 内側の Call は元実装では cast の本体そのもの。同じ型を付ける。
            if (result) {
                if (auto inner = d->base.ref(a); inner && !inner->type) {
                    inner->type = result;
                }
            }
            if (auto args = d->arguments.ref(a)) {
                for (auto& arg : args->arguments) {
                    type_of_expr(arg.ref(a)->value);
                }
            }
        }
        else if (auto call = e.as_any<Call>()) {
            auto d = call.ref(a);
            if (auto args = d->arguments.ref(a)) {
                for (auto& arg : args->arguments) {
                    type_of_expr(arg.ref(a)->value);
                }
            }
            result = type_of_stream_call(call);
            if (!result) {
                auto ct = type_of_expr(d->callee);
                if (auto ft = ct.as_any<FunctionType>()) {
                    result = ft.ref(a)->return_type;
                }
                else if (ct.as_any<EnumType>() || ct.as_any<StructType>()) {
                    // Enum(x) / Format(..) の形。元実装は callee を型リテラルに
                    // 直して Cast にする (call_to_cast)。型はその型自身。
                    result = ct;
                }
            }
        }
        else if (auto cond = e.as_any<ConditionalExpr>()) {
            result = type_of_conditional(cond);
        }
        else if (auto c = e.as_any<Cond>()) {
            // 三項。両辺が揃えばその型。
            auto d = c.ref(a);
            type_of_expr(d->cond);
            auto t = type_of_expr(d->then);
            auto f = type_of_expr(d->els);
            result = common_type(t, f);
        }
        else if (auto sz = e.as_any<Sizeof>()) {
            type_of_expr(sz.ref(a)->target);
            auto t = a.make<IntType>(loc);
            auto d = t;
            d->bit_size = 64;
            d->is_signed = false;
            d->endian = Endian::unspec;
            result = t;
        }
        else if (auto av = e.as_any<Available>()) {
            type_of_expr(av.ref(a)->target);
            result = a.make<BoolType>(loc);
        }
        else if (auto ma = e.as_any<MemberAccess>()) {
            result = type_of_member_access(ma);
        }
        else if (auto oc = e.as_any<OrCond>()) {
            // match の分岐条件を | でつないだ形。全条件の共通型で、範囲は基底の
            // 型で比べる。元実装の typing_or_cond / OrCond_common_type に当たる。
            auto d = oc.ref(a);
            Node<Type> ty;
            bool ok = true;
            for (auto& c : d->conds) {
                auto t = type_of_expr(c);
                if (!t) {
                    ok = false;
                    continue;  // 型付け自体は全条件に回す
                }
                if (!ok || !ty) {
                    ty = t;
                    continue;
                }
                auto merged = common_type(ty, t);
                if (!merged) {
                    if (auto r = ty.as_any<RangeType>()) {
                        merged = common_type(r.ref(a)->base_type, t);
                    }
                }
                if (!merged) {
                    if (auto r = t.as_any<RangeType>()) {
                        merged = common_type(ty, r.ref(a)->base_type);
                    }
                }
                if (!merged) {
                    ok = false;
                    continue;
                }
                ty = merged;
            }
            if (ok) {
                result = ty;
            }
        }
        else if (auto import_ = e.as_any<Import>()) {
            // 読み込んだ Module の struct 型。メンバアクセスの左辺になる。
            if (auto* r = tables.table<ImportResolution>().get(import_)) {
                result = struct_type_of_module(r->module);
            }
        }

        in_progress_.erase(e.id());
        if (result) {
            e.ref(a)->type = result;
        }
        return result;
    }

    // config が指す組み込みの宣言木。enum メンバの値は元実装の Evaluator が
    // IOOperation に与える整数と同じ (eval.h:402): big=0 little=1 native=2 /
    // msb=0 lsb=1。ここで合成しておくと、メンバアクセスの解決も定数畳み込みも
    // ユーザー定義の enum と同じ経路で流れる。
    Node<Module> Typer::builtin_module() {
        if (builtin_module_) {
            return builtin_module_;
        }
        lexer::Loc loc{};  // file = lexer::builtin (0)
        auto make_enum = [&](std::string_view name, std::initializer_list<std::string_view> members) {
            auto en = a.make<Enum>(loc);
            en->name = a.make<Ident>(loc, std::string(name));
            auto et = a.make<EnumType>(loc);
            et->base = en;
            en->enum_type = et;
            std::size_t value = 0;
            for (auto& m : members) {
                auto mem = a.make<EnumMember>(loc);
                mem->name = a.make<Ident>(loc, std::string(m));
                mem->belong = en;
                mem->value = a.make<IntLiteral>(loc, NodeData<Literal>{}, brgen::nums(value));
                en->members.push_back(mem);
                value++;
            }
            return en;
        };
        auto mod = a.make<Module>(loc);
        mod->statements.push_back(make_enum("endian", {"big", "little", "native"}));
        mod->statements.push_back(make_enum("bit_order", {"msb", "lsb"}));
        builtin_module_ = mod;
        return builtin_module_;
    }

    // 型の位置に書かれた名前。指している宣言の型を base に入れる。
    void Typer::resolve_ident_type(Node<IdentType> t) {
        auto d = t.ref(a);
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
