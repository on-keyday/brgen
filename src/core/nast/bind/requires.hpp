/*license*/
#pragma once
#include "../nodes.h"
#include "typer.hpp"

#include <core/common/error.h>

// requires 推論。Format / Function ごとに「外から与えられる文脈に何を要求するか」
// を集めて Requirements 表に置く。要求は 2 種類:
//
//   入力ストリームの能力
//     peek     input.peek を使う。先読みできない逐次入力では buffering が要る
//     backward input.backward を使う。巻き戻し
//     remain   input.remain / input.scope_length を使う。長さ既知の入力が要る
//     offset   input.offset / input.bit_offset を使う。位置の追跡が要る
//   state 変数の使用
//     state_read / state_write。トップレベルの StateVariable 単位
//
// 集め方は効果推論: 自分の本体で直接使ったものに、参照している format / 呼んで
// いる fn の要求を重ねる (再帰 format があるので不動点まで回す)。要求は消える
// こともある: field の引数で input = input.subrange(len) と長さを確立した
// ストリームに束ね直すと、呼び先の remain はその場で満たされて伝播しない。
// peek / backward / offset は subrange 越しでも下の入力の能力なので素通し。
//
// get / put / subrange 自体は要求にしない。get はストリームの最低線で、
// subrange はどの実体でも「n バイト読む」で作れる。
//
// 検査はしない (要求と提供の突き合わせはバックエンドの仕事)。ここは format の
// 側の事実を測って表に置くだけ。ebmgen の
// `TODO: strictly analyze state variable usage in ast` (statement.cpp) が
// 要求していた per-function の使用解析に当たる。

namespace brgen::nast::bind {

    struct RequiresInference {
        Arena& a;
        SideTables& tables;
        Typer& typer;

        // 表に置いた owner (Format / Function) の数。計測用。
        std::size_t inferred = 0;

        void run(const std::vector<Node<Module>>& modules);
    };

}  // namespace brgen::nast::bind
