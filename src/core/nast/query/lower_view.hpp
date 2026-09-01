/*license*/
#pragma once
#include "../bind/pipeline.h"

#include <string>
#include <utility>
#include <vector>

// そのノードに当てはまる lowering 規則を当てて、結果を .bgn の綴りで返す。
// 当たる規則が無ければ空。
//
// バッファと位置は呼ぶ側が決めるものなので、ここでは `buf` / `o` を仮に置く。

namespace brgen::nast::query {

    struct Lowered {
        std::string text;
        // 作ったノード。ラベルと id。field の読み書きは表に載らない
        // (結果が呼ぶ側のバッファ名にも依るのでキーにならない) ので、
        // 追いかけたい側はここから id を取る。
        std::vector<std::pair<std::string, NodeAny>> made;
    };

    Lowered lower_of(Program& p, NodeAny n);

    inline std::string lower_text(Program& p, NodeAny n) {
        return lower_of(p, n).text;
    }

}  // namespace brgen::nast::query
