# transform/

convert/で一通り変換された EBM をさらに変換するための機構。
transform/という名前なのは optimize/というには全然最適化ではなく変換しているだけというのが実情に近いからである。

## control flow graph

変換の際に control flow graph を使っている箇所があり、また`-c`フラグで control flow graph を dot 形式で出力可能である。ただし、なんちゃって CFG な面もまだあるため要改善である。
