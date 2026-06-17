# Assert の失敗時挙動を関数文脈で出し分ける

## 日付

- 判断時期: 2026-06-18
- 文書化: 2026-06-18

## 判断

`Assert`（format 本体の検証式、および関数本体に書かれた式文）の失敗時挙動を、
EBM 変換時（`assert_statement_body`）に `generate_type` で出し分ける。

- **Normal 関数文脈**: 条件が偽なら関数の戻り値型のデフォルト値を `return`
  （例: `-> bool` の検証関数なら `false` を返す）
- **Encode / Decode 文脈**: 条件が偽なら error report（encoder/decoder のエラーチャネル）
- **void を返す Normal 関数**: 失敗を通知する値が無いため現時点では非対応（変換エラーで表面化）

## 動機

- **write once generate any language code**: 同じ `Assert` ノードでも、
  純粋関数の中では「戻り値で失敗を表す」、coder の中では「error を返す」と
  意味が変わる。この出し分けを EBM 変換層で一度行えば全 ebm2* に効く。
  codegen 側に文脈判定を重複させない。
- きっかけは utf8.bgn の `isUTF8`（`-> bool`）。本体の式文（`i + 1 < data.length` 等）が
  常に error report に lower され、`return Err(...)` が bool 関数に生成されて
  コンパイルエラー（Rust E0308）になっていた。

## 具体例

- `fn isUTF8(data :[]u8) -> bool` 内の `data[i+1] == (0x80..=0xBF)`:
  Normal 文脈 → 偽なら `return false`
- `format Utf8String: data :[..]u8; utf8.isUTF8(data) == true`:
  decode 文脈 → 偽なら decode error

`Return` 文の変換（`convert_statement_impl(ast::Return)`）が既に
`get_current_generate_type()` で Normal / coder を出し分けており、Assert も同じ前例に倣う。

## これは X を意味しない

- 「Assert は常に error」ではない。文脈で意味が変わる。
- codegen 側で文脈判定するのではない。EBM 変換層で決め切る（[[0001-is-mutable-in-ebm]] と同じ思想）。
- void Normal 関数での Assert の意味（ガード節として早期 return するか等）を確定したわけではない。
  正当なユースケースが出た時点で別途設計する。それまでは非対応エラーで表面化させる。

## 代替案

- **codegen 側で文脈判定**: 各 ebm2* が「今 bool 関数の中か coder の中か」を判定して
  出し分ける。全 backend に同じロジックが重複し、目標 3 に反する。却下。
- **void Normal でも黙って早期 return**: 検証の意味（失敗通知）が silent に失われ、
  「検証したつもり」バグの温床になる。ガード節が欲しければ `if (!cond): return` と
  明示的に書けるので、Assert に寛容な void フォールバックは持たせない。
