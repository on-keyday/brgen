/*license*/
#pragma once
#include "../../bind/pipeline.h"

#include <map>
#include <string>

// 解析と lowering の結果を印字して眺めるための道具。
//
//   nast_probe size    <file.bgn>...   型の幅と幅の式
//   nast_probe endian  <file.bgn>...   field ごとに効いているバイト順
//   nast_probe lower   <file.bgn>...   三項 / match / 範囲比較 / 整数とバイト列
//
// ファイルが 1 つなら明細、2 つ以上なら集計。合成した木の正しさは、綴りに
// 戻して眺めるのが一番速い (docs/size_and_lowering.md 末尾)。
//
// **実行の入り口は 1 つ、ソースはモードごとに 1 ファイル。** 覚える名前は
// `nast_probe` だけにしたいが、木を見て「何が見られるか」も分かってほしい
// ので、分ける場所を分けてある。

namespace brgen::nast::probe {

    // 集計。キーは `群: 内訳` の形で、先頭が空白の行は割合の母数に入れない。
    using Hist = std::map<std::string, std::size_t>;

    void run_size(Program& p, bool detail, Hist& hist);
    void run_endian(Program& p, bool detail, Hist& hist);
    void run_lower(Program& p, bool detail, Hist& hist);

}  // namespace brgen::nast::probe
