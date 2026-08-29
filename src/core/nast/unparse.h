/*license*/
#pragma once
#include "nodes.h"

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

namespace brgen::nast {

    std::string unparse(Arena& a, Node<Module> mod);

}  // namespace brgen::nast
