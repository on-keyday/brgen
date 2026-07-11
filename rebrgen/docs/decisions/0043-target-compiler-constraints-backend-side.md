# ターゲット言語のコンパイラ制約は共有 knob ではなく backend 側の解析で吸収する

## 日付

- 判断時期: 2026-07-11 (ebm2java 追加時)
- 文書化: 2026-07-11

## 判断

単一のターゲット言語のコンパイラ制約 (例: javac の到達可能性検査) に
しか意味を持たない対応は、default_codegen_visitor の共有 Config への
knob 追加ではなく、その backend 内のフック (必要なら IR 上の静的解析)
で解決する。

## 動機

- 共有 Config への knob 追加方針 (0016 / feedback として運用中) は
  「複数言語が再利用しうる注入点」を対象とする。単一言語の quirk を
  knob 化すると Visitor.hpp が言語別 if の墓場になり、knob の意味が
  他 backend 開発者 (と AI) にとってノイズになる。
- backend 側で解くと、対象言語の仕様 (この場合 JLS) に対する忠実さを
  その backend の責任範囲に閉じ込められる。

## 具体例

ebm2java で、EBM が全分岐 return の if/else や break なし無限ループの
後に置く trailing RETURN が javac の "unreachable statement" (JLS 14.21)
に拒否された。当初 `plain_else_guard` (plain else を `else if (<guard>)`
に変える knob) を共有 Visitor.hpp に追加したが撤回し、代わりに
`src/ebmcg/ebm2java/visitor/Statement_BLOCK_class.hpp` で javac の
到達可能性解析を EBM IR 上にミラーし、到達不能文を emit 段階で drop する
方式にした。解析は emitter が実際に辿る表現 (MATCH の lowered_if_statement、
LOOP の lowered_statement) と同じ側を見る必要がある点に注意。

## これは X を意味しない

- 「knob 追加をやめる」ではない。複数言語で再利用可能な注入点は引き続き
  共有 Config に足す (0016 の方針は不変)。境界は「その knob の説明文が
  特定言語名なしで書けるか」。
- 「backend 側の解析は自由にやってよい」ではない。emit 判定 (lowered vs
  高レベル) を再実装する解析は、default visitor の判定とズレると
  二重 emit / 欠落を生むため、対象を実際に emit される形に限定する。

## 代替案

- `plain_else_guard` knob (実装後に撤回)。却下理由: Java 以外に使い道が
  なく、しかも javac の到達可能性は else の問題だけではない (無限ループ
  後の文も同罪) ため、knob では穴が残り結局解析が必要だった。
