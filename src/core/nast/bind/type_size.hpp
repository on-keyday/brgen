/*license*/
#pragma once
#include "../node/nodes.h"

#include <core/common/error.h>
#include <optional>
#include <utility>
#include <vector>
#include <unordered_set>

// 型のビット幅。TypeSize 表 (over Type) に置く。
//
//   kind = fixed    ビット数が定数。bits が有効
//   kind = dynamic  実行時に決まる。bits_expr に幅の式が入る (書けたとき)。
//                   長さが式の配列は `要素幅 * 長さ` に、和は `+` になる。
//                   型パラメータは sizeof(<T>) * 8 — instantiation 前でも
//                   幅を運べるので、monomorphize を待たずに扱える。
//                   分岐で幅が揃わない場合は書けないので null になる
//                   (「どの分岐を通ったか」は式では表せない)
//   kind = unknown  決まらない (末尾までの配列、循環、型の付かない入力)
//
// これは lowering の前段。ビットフィールドの畳み込み (どの隣接 field が同じ
// backing byte に入るか) も、連続する固定長 IO のまとめも、まず「その型は
// 何ビットか」が要る。ebmgen だと transform の中で IO statement の
// size.unit を見ながらやっているが、幅そのものは IO の表し方に依らない
// 型の性質なので、front end のこちらに置ける。
//
// 語彙を 3 つに絞ったのは、最初の消費者 (bit field 畳み込み / 連続 IO の
// まとめ) が「固定で何ビットか」しか要らないため。ebmgen の SizeUnit は
// 8 値あり BYTE_DYNAMIC / ELEMENT_DYNAMIC / DYNAMIC を分けているが、あの
// 区別は「実行時のバイト数をどう計算して出すか」のためのもので lowering の
// 都合。解析の表に持ち込むと消費者のいない語彙が増える。
//
// 分岐は木の再帰で決まる。binder が分岐ごとの field を InnerStruct 表に
// 置いてくれているので、各分岐を畳んで揃えばそれが幅。CFG が要るのは幅では
// なくビットの畳み込み (どの経路でも 8 の倍数に達するか) のほうで、これは
// 別の問い。Match の値による網羅だけは見ていない (既定の分岐が無ければ
// 「どれも通らない経路」があるものとして扱う)。

namespace brgen::nast::bind {

    struct SizeAnalysis {
        Arena& a;
        SideTables& tables;
        LocationError& err;

        // 表に置いた型の数。計測用。
        std::size_t analyzed = 0;

        void run();

        // 1 つの型を解く。表にあればそれを返す。
        TypeSize size_of(Node<Type> t);

        // 循環している型 (format A が A を含む) を検出する。解いている最中の
        // ものにもう一度入ったら unknown で切る。
        std::unordered_set<std::uint32_t> in_progress;

        TypeSize field_size(Node<Field> f, lexer::Loc loc);
        Node<Expr> size_of_value(Node<Field> f, lexer::Loc loc);
        TypeSize format_size(Node<Format> fmt);
        TypeSize inner_size(Node<BodyStatement> block);
        TypeSize put(Node<Type> t, TypeSize s);
        TypeSize add(TypeSize l, TypeSize r, lexer::Loc loc);
        TypeSize merge_branch(std::optional<TypeSize> acc, TypeSize s);

        // 合成したノードはアリーナに積まれるが、木からは辿れず TypeSize 表
        // からだけ来る。組み立ては node/build.h の Builder。
        Node<Expr> as_expr(TypeSize s, lexer::Loc loc);
        Node<Expr> match_subject(Node<ConditionalExpr> base);
        template <class Cands>
        Node<Expr> branch_expr(const Cands& candidates, Node<Expr> subject, lexer::Loc loc,
                               auto&& width_of);
    };

}  // namespace brgen::nast::bind
