/*license*/
#pragma once
#include "../node/nodes.h"
#include "typer.hpp"

#include <core/common/error.h>

// union の合流の決定構造。分岐で宣言された同名 field (UnionType) ごとに、
// 候補の型の集合と、common_type で畳める群 (クラスタ) を UnionLayout 表に置く。
//
//   member_types   候補 field の相異なる型 (equivalent で同一視、出現順)
//   cluster_types  member_types と並行。その型が属するクラスタの合流型。
//                  同じノードを指す = 同じクラスタ
//
// 読み方: member_types が 1 つなら strict (単一型)、cluster_types の相異なる
// ノードが 1 つなら common (全体が 1 つの共通型に畳める)、複数残れば
// uncommon (variant で持つしかない)。ebmgen の derive_property_type
// (convert/union_property.cpp) の STRICT_TYPE / COMMON_TYPE / UNCOMMON_TYPE
// 決定に当たる。あちらは EBM 変換の中で型検出・クラスタリング・getter/setter
// 式の実体化を一体でやっているが、決定構造は型だけで決まるのでここへ移した。
// 式の実体化 (encode/decode 文脈での条件式の複製、without-field 条件の OR
// 畳み込み) は lowering 側に残る。member×branch の対応表は持たない —
// candidates の順序と型の同一視から導出できる。
//
// クラスタの作り方は ebmgen の clustering_properties の写し: 型どうしに
// common_type が存在すれば辺を張り、先行する最小の隣へ合流させる
// (推移閉包は取らない)。合流型はクラスタ内を出現順に common_type で畳む。
//
// generic format の body の union (value :T など) は型パラメータのまま
// 解析される。instantiation ごとの layout (Either[u16,u32] は common だが
// Either[Foo,Plain] は uncommon) は monomorphize の後段の話。

namespace brgen::nast::bind {

    struct UnionLayoutAnalysis {
        Arena& a;
        SideTables& tables;
        Typer& typer;
        LocationError& err;

        // 表に置いた union の数。計測用。
        std::size_t analyzed = 0;

        void run();
    };

}  // namespace brgen::nast::bind
