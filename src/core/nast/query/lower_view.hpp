/*license*/
#pragma once
#include "../bind/pipeline.h"

#include <string>

// そのノードに当てはまる lowering 規則を当てて、結果を .bgn の綴りで返す。
// 当たる規則が無ければ空。
//
// バッファと位置は呼ぶ側が決めるものなので、ここでは `buf` / `o` を仮に置く。

namespace brgen::nast::query {

    std::string lower_text(Program& p, NodeAny n);

}  // namespace brgen::nast::query
