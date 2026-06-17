# range-membership 比較を RANGE_EQUAL expression + lowered_expr で表す

## 日付

- 判断時期: 2026-06-18
- 文書化: 2026-06-18

## 判断

`x == (a..=b)`（値が範囲に入るかの比較）を、専用の ExpressionKind `RANGE_EQUAL` に変換する。
`RANGE_EQUAL` は tested value (`value`)・範囲式 (`range_expr`)・展開形 (`lowered_expr` =
`a<=value && value<=b`) を持つ。default codegen は lowered_expr を出力し、backend は必要なら
native な範囲表現に override できる。

## 動機

- **write once generate any language code**: 「範囲メンバーシップ」という意味を IR に残せば、
  各 backend が言語ごとの最適表現（Rust `(a..=b).contains(&x)`、Go の `a <= x && x <= b`、
  将来の switch/range 構文など）を選べる。意味を捨てて `BINARY_OP less_or_eq + and` に潰すと、
  backend は「これは範囲判定だった」と知れず native 表現を選べない。
- **lowered_expr パターンとの一貫性**: CONDITIONAL / AVAILABLE / ENUM_IS_DEFINED と同じく
  「意味ノード + 展開済み lowered」の形（[[0011-lowered-io-statements]] と同じ思想）。
  default は lowered で必ず動き、最適化は任意。
- きっかけは utf8.bgn の `isUTF8` 内 `data[i] == (0x80..=0xBF)`。一般の `ast::Binary` の `==` が
  `convert_equal` を経由せず、RANGE を BINARY_OP equal の裸 RHS にしていたため、codegen が
  右辺を空に出してコンパイルできなかった（`if (data[i] == ) {`）。

## 具体例

- `convert_equal_impl` は range 展開（`a<=x && x<=b`）を組んだ上で `RANGE_EQUAL` でラップする
  （value=x, range_expr=range, lowered_expr=展開）。
- `ast::Binary` の `==` は、片側が RANGE のときだけ `convert_equal` 経由にして RANGE_EQUAL を作る。
  通常の等値は従来の cast + BINARY_OP equal のまま。
- match の range arm（`0x80..=0xBF => ...`）も `derive_match_lowered_if` → `convert_equal` 経由なので
  同じく RANGE_EQUAL になり、lowered if の条件として使える。
- default codegen hook（default_codegen_visitor の Expression_RANGE_EQUAL）は
  `lowered_expr` を visit して `a<=x && x<=b` を出力。ebm2rust ではこれで utf8 が round-trip。

## これは X を意味しない

- 「あらゆる比較を専用ノード化する」ではない。意味が明確で backend 最適化の余地がある
  range-membership に限る。
- backend が native range 表現を必ず実装する義務はない。default の lowered_expr で動く。
  native 実装は各 backend の任意 override。

## 代替案

- **binary `==` に直展開（BINARY_OP less_or_eq + logical_and）**: 動くが「範囲判定」の意味が
  IR から消え、backend が native range 表現を選べない。目標 3 の観点で劣る。却下。
- **RANGE を BINARY_OP equal の裸 RHS のまま渡す**: codegen が RANGE を式として出せず右辺が空に
  なり壊れる（実際に起きた不具合）。却下。
