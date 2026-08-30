/*license*/
#pragma once
#include "../node/nodes.h"

// バックエンドが呼びたいときに呼ぶ変換の置き場。
//
// EBM (ebmgen/transform) は変換を 13 段のパイプラインとして持ち、しかも
// 三項の文形式のように「どの言語が要るか分からないもの」は全部先に作って
// IR に載せている (convert/expression.cpp の make_conditional は
// CONDITIONAL と CONDITIONAL_STATEMENT を毎回両方作る)。消費者を知る前に
// 形を確定するしかない作りだったためで、代償は 3 つ:
//
//   - 誰も使わない形も必ず作る (後段の remove_unused_object が掃除する)
//   - lowered は通常の子として繋がらないので汎用走査で辿られない
//   - 形が増えるたび IR が太る
//
// nast は front と back が同じ木を共有しているので、変換を「前に走った段」
// ではなく「バックエンドが呼ぶ道具」にできる。ここはそのための置き場。
//
// 規約:
//
//   - 足すだけ。元の木は書き換えないし落とさない。
//     (docs/exit_and_reversibility.md の復元性規則 1)
//   - 合成したノードの loc は由来のものを使う。規則 2
//   - 結果は由来をキーにした side table に置く。2 度呼んでも同じノードが
//     返るし、由来から辿れる。規則 3
//   - 「どこに置くか」は返す側では決めない。呼ぶ側が置き場を持っている。
//
// 呼ぶ側が置き場を持つ、というのが EBM との一番の違いになる。三項の文形式は
// 一時変数を宣言する場所を要求するが、式は木のどこにでも出るので、ホイスト
// 先を知っているのはコードを出している側だけ。

namespace brgen::nast::lowering {

    struct Context {
        Arena& a;
        SideTables& tables;
    };

}  // namespace brgen::nast::lowering
