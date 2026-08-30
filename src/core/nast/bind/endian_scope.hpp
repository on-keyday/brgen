/*license*/
#pragma once
#include "../node/nodes.h"

#include <core/common/error.h>

// バイト順のスコープ。`input.endian = ...` がどの field に効くかを解いて、
// FieldEndian 表 (over Field) に置く。
//
// 規則 (rebrgen converter.hpp:462 に書かれているもの):
//
//   input.endian は**字句スコープ**。書いた文からその block の末尾まで効き、
//   呼び出しグラフを辿って入れ子 format には及ばない。block は format の
//   本体・fn の本体・if/match/loop の本体。トップレベルの代入は block では
//   ないので、そこから先のファイル全体に効く。
//
// 型に綴りとして書かれた endian (型記述子が持つ) はスコープより強い。
// 未指定 (`unspec`) のときにスコープが埋める。スコープにも何も無ければ
// big — 言語の既定。
//
// **値は定数とは限らない。** `input.endian = endian.is_big ? config.endian.big :
// config.endian.little` (bpf.bgn) のように実行時に決まる形がある。そのとき表は
// **式ではなく代入 (SpecifyOrder) を指す**。式は代入の位置で 1 回だけ評価される
// ものなので、field ごとに展開すると (a) 参照している field が先に進んでいれば
// 別の答えになり、(b) 同じ三項を field の数だけ焼くことになる。EBM が
// ENDIAN_VARIABLE という文に落としているのも同じ理由。
//
// 格納される値の型も揃っていない (bpf/elf は config.endian.* の列挙、
// omg_cdr は bool)。EBM の IS_LITTLE_ENDIAN が little_endian_value を持つのは
// そのため。代入の位置で正規化すれば、判定は定数との普通の比較で足りる。
//
// なお ebmgen の動的 endian は未完 (converter.cpp:45-63 に明記: set_on_function
// が呼ばれないため current_dynamic_endian が入らず、動的 endian が全部 native
// として生成される)。写せる実装は無い。
//
// 元実装 (middle/typing.cpp:1993) はトップレベルの最後の 1 つを取って
// ファイル全体に効かせる。こちらは書かれた位置から効かせる (字句スコープの
// 定義に素直な読み方)。トップレベルの指定が全 format より前にあるコーパスでは
// どちらも同じ結果になる。

namespace brgen::nast::bind {

    struct EndianScope {
        Arena& a;
        SideTables& tables;
        LocationError& err;

        // 表に置いた field の数。計測用。
        std::size_t analyzed = 0;
        // そのうち実行時に決まるもの。
        std::size_t dynamic = 0;

        void run(const std::vector<Node<Module>>& modules);

        struct State {
            Endian endian = Endian::unspec;
            Node<SpecifyOrder> dynamic;  // 実行時に決まるならその代入
        };

        State current;

        void walk_body(Node<Body> body);
        void walk_statement(Node<Statement> s);
        void apply(Node<Field> f);
        bool set_from(Node<SpecifyOrder> order);
    };

}  // namespace brgen::nast::bind
