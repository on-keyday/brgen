/*license*/
#pragma once
#include "nodes.h"

#include <code/loc_writer.h>
#include <string>
#include <vector>

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
// 書き出しは futils の LocWriter に載せる。行とインデントを構造として持つので
// 桁を数えなくて済み、それぞれの断片が **どのノードから出たか** を一緒に記録
// できる。この対応表 (UnparseResult::spans) が source map の下地になる。
// rebrgen の ebmcodegen が同じ Writer を使っている (docs/lessons_from_ebmcodegen.md)。

namespace brgen::nast {

    // 出力の断片と、それを出したノードの対応。行は 1 起点、桁は 0 起点で、
    // どちらもインデントを展開する前の値。
    struct UnparseSpan {
        NodeAny node;
        std::size_t begin_line = 0;
        std::size_t begin_col = 0;
        std::size_t end_line = 0;
        std::size_t end_col = 0;
    };

    struct UnparseResult {
        std::string text;
        std::vector<UnparseSpan> spans;
    };

    // 木を .bgn に戻す。由来の対応表も要るなら unparse_with_spans を使う。
    std::string unparse(Arena& a, Node<Module> mod);
    UnparseResult unparse_with_spans(Arena& a, Node<Module> mod);

}  // namespace brgen::nast
