/*license*/
#pragma once
#include "../nodes.h"
#include "typer.hpp"

#include <core/common/error.h>

// requires 推論。Format / Function ごとに「外から与えられる文脈に何を要求するか」
// を decode / encode の方向別に集めて Requirements 表に置く。要求は 2 種類:
//
//   入力ストリームの能力
//     peek     input.peek を使う。先読みできない逐次入力では buffering が要る
//     backward input.backward を使う。巻き戻し
//     remain   input.remain / input.scope_length を使う。長さ既知の入力が要る
//     offset   input.offset / input.bit_offset を使う。位置の追跡が要る
//   state 変数の使用
//     state_read / state_write。トップレベルの StateVariable 単位
//
// 方向の付け方は docs/requires_direction.md の段階 1 (保守的な分離):
//
//   as_is body の入力能力    decode 側だけ。encode は書くだけで、先読み・残量・
//                            巻き戻しは入力の性質 (encode 時の入力条件式の意味論
//                            は未決のまま踏み込まない)
//   as_is body の state      両方向。文は encode / decode の両方で実行される
//   custom encode/decode fn  その fn の要求をその方向だけへ
//   field :Format            方向を保って伝播 (子の decode は親の decode へ、
//                            子の encode は親の encode へ)
//   Function                 方向を持たない。集めたものを正準とし、表には
//                            両側に写す — 呼ばれた方向でその能力が要る
//
// 集め方は効果推論: 自分の本体で直接使ったものに、参照している format / 呼んで
// いる fn の要求を重ねる (再帰 format があるので不動点まで回す)。要求は消える
// こともある: field の引数で input = input.subrange(len) と長さを確立した
// ストリームに束ね直すと、呼び先の remain (decode 側) はその場で満たされて
// 伝播しない。peek / backward / offset は subrange 越しでも下の入力の能力なので
// 素通し。
//
// get / put / subrange 自体は要求にしない。get はストリームの最低線で、
// subrange はどの実体でも「n バイト読む」で作れる。encode 側の出力能力
// (out_offset / out_backward 等) は語彙ごと未着手 (段階 2)。
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
