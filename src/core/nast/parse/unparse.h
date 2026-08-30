/*license*/
#pragma once
#include "../node/code_writer.h"

#include <string>

// parse が組んだ木から .bgn を書き戻す (逆変換)。
//
// 目標は原文の完全再現ではなく、**再 parse したときに同じ形の木になる**こと。
// コメントと空行と桁は残らない。インデントは 4 空白に正規化する。
// 検証は nast_unparse_test が parse -> unparse -> 再 parse -> structural 比較
// (compare.h) で行い、さらに unparse をもう一度かけてテキストが動かないこと
// (不動点) を見る。
//
// parse が形を畳んで原文が一意に戻らないところは、再 parse で同じ木になる側へ
// 寄せて書く:
//   - Metadata の代入形と 1 引数の呼び出し形 (config.url = x / config.url(x))
//     はどちらも同じ木なので代入形で書く
//   - <u8>(x) と u8(x) は同じ Cast。中の型リテラルの is_explicit で書き分ける
//   - match 分岐の `=> 文` と 1 文だけのブロックは同じ木なので、1 行に置ける
//     文なら `=>` で書く
//   - enum メンバの値は書かれなかったものも parse が合成する。raw_expr が
//     あるときだけ `= 値` を書き、合成分は再 parse に作り直させる
//
// これができるのは解析結果が side table にあり、木が原文の形のままだから。
// 落ちる情報が見つかったらノードに持たせて塞ぐ (SpecifyOrder の name が実例)。
//
// 書き出しは node/code_writer.h の CodeWriter に載せる。行とインデントを
// 構造として持つので桁を数えなくて済み、それぞれの断片が **どのノードから
// 出たか** も記録できる。同じ Writer をバックエンド側でも使う。

namespace brgen::nast {

    // 木を .bgn に戻す。由来の対応表 (CodeOutput::spans) も要るなら
    // *_with_spans を使う。型は node/code_writer.h にある。
    std::string unparse(Arena& a, Node<Module> mod);
    CodeOutput unparse_with_spans(Arena& a, Node<Module> mod);

    // 木の一部だけを書く。Module でなくてよく、文 / 式 / 型 / 名前のどれでも
    // その位置での書き方で出る (式は式として、型は型として)。
    //
    // 診断や hover に式 1 つを見せる、生成コードのコメントに元の宣言を添える、
    // 差分を取るのに部分木だけ文字列にする、といった用途向け。全体を書いて
    // から切り出すのではなく、そのノードから始める。
    //
    // 出るのはそのノード単体で、インデントは 0 から始まる。周りの文脈
    // (何段目のブロックにいたか) は持たない。
    std::string unparse_node(Arena& a, NodeAny n);
    CodeOutput unparse_node_with_spans(Arena& a, NodeAny n);

}  // namespace brgen::nast
