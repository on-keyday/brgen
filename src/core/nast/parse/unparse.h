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
// **再 parse できるのは parse が組んだ木の範囲**である。lowering が合成した
// ノードには .bgn の構文が無いものがあり (IsLittleEndian が今のところそう)、
// それらの綴りは眺めるためのもので再 parse を通らない。合成ノードは木からは
// 辿れず side table からしか来ないので、往復の検証がそこへ入ることはない。
// lowering が増えれば同じ性質のノードも増える。
//
// 書き出しは node/code_writer.h の CodeWriter に載せる。行とインデントを
// 構造として持つので桁を数えなくて済み、それぞれの断片が **どのノードから
// 出たか** も記録できる。同じ Writer をバックエンド側でも使う。

namespace brgen::nast {

    struct UnparseOption {
        // bind/receiver が足した暗黙のレシーバも綴る (`len` を `self.len` と
        // 出す)。何がレシーバを取ると判定されたかをそのまま読むためのもの。
        // 再 parse は通るが `is_explicit` が変わるので、往復の検証には使わない。
        bool explicit_self = false;
    };

    // 木を .bgn に戻す。由来の対応表 (CodeOutput::spans) も要るなら
    // *_with_spans を使う。型は node/code_writer.h にある。
    std::string unparse(Arena& a, Node<Module> mod, UnparseOption opt = {});
    CodeOutput unparse_with_spans(Arena& a, Node<Module> mod, UnparseOption opt = {});

    // 木の一部だけを書く。Module でなくてよく、文 / 式 / 型 / 名前のどれでも
    // その位置での書き方で出る (式は式として、型は型として)。
    //
    // 診断や hover に式 1 つを見せる、生成コードのコメントに元の宣言を添える、
    // 差分を取るのに部分木だけ文字列にする、といった用途向け。全体を書いて
    // から切り出すのではなく、そのノードから始める。
    //
    // 出るのはそのノード単体で、インデントは 0 から始まる。周りの文脈
    // (何段目のブロックにいたか) は持たない。
    std::string unparse_node(Arena& a, NodeAny n, UnparseOption opt = {});
    CodeOutput unparse_node_with_spans(Arena& a, NodeAny n, UnparseOption opt = {});

    // 組み立て途中の Writer に貼るための形。文字列に落とさないので、
    // どの断片がどのノードから出たかがそのまま残る。
    //
    //   return CODELINE_AT(m, unparse_writer(c.arena(), m));  // 由来が消える
    //   return unparse_writer(c.arena(), m);                  // 残る
    //
    // バックエンドが .bgn の一部をそのまま出したいとき (未対応の構文を
    // コメントで添える、元の宣言を残す) はこちらを使う。
    CodeWriter unparse_writer(Arena& a, NodeAny n, UnparseOption opt = {});

}  // namespace brgen::nast
