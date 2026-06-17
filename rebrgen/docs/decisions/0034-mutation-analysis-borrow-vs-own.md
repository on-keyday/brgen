# mutation analysis で借用 / 所有を決定する（is_mutable の一般化）

## 日付

- 判断時期: 2026-06-18
- 文書化: 2026-06-18
- ステータス: 提案（着手前の方針記録）

## 判断

[[0001-is-mutable-in-ebm]] の self mutation 解析（`FunctionAttribute.is_mutable`）を、
**parameter / local variable / field** の mutation 解析へ一般化する。
各 decl が代入（`ASSIGNMENT` / `APPEND` 等）の target として変更されるかを
EBM 変換時に解析して `is_mutated` 属性に焼き、codegen が
**借用 (`&T`) / 可変借用 (`&mut T`) / 所有** を決定する。

## 動機

- **write once generate any language code**: Rust の `&[u8]` vs `Vec<u8>`、
  `&` vs `&mut` vs 所有、C++ の `const&` vs 値渡しなど、借用/所有の表現は言語ごとに
  異なるが、「その decl が書き換えられるか」という判定ロジックは共通。
  EBM に 1 次情報として持たせれば一箇所で済む。self 版（0001）の自然な拡張。
- きっかけは utf8.bgn の `isUTF8(data :[]u8)`。引数を読むだけなのに所有型
  （`Cow<'a,[u8]>` / `Vec<u8>`）で生成され、(1) 自由関数に lifetime `'a` が無く Rust E0261、
  (2) 所有受けのため呼び出し `isUTF8(self.data)` が借用フィールドを move して E0507、
  になっていた。mutation が無いと分かれば `&[u8]` で受けられ、両方根本から消える。

## 具体例

- `fn isUTF8(data :[]u8) -> bool`: data を変更しない → `&[u8]`
- vector へ append する setter の引数 / 変更されるローカル: `&mut` または所有
- 0001 の self mutation は、この一般化された解析の「receiver が self の場合」の特殊ケースとして
  位置づけられる（`has_self` → 任意 decl への一般化）。

## 実装スケッチ

1. EBM: `PARAMETER_DECL` / `VariableDecl`（必要なら `FieldDecl`）に `is_mutated` 属性を追加
   （構造変更のため `update_ebm` と全 ebm2* 再生成を伴う）。
2. convert: `has_self` / `may_mark_self_modified` を「代入 target が指す decl を特定して
   mutated マークする」一般形へ拡張。self と同じく関数 scope で集計し、変換後に各 decl へ反映。
3. codegen: `param_visitor` 等が `is_mutated` を見て借用/可変借用/所有を選ぶ。

## これは X を意味しない

- borrow checker を EBM 上で再実装するのではない。持つのは「mutation の有無」という 1 次情報だけ。
  lifetime の詳細な伝播や借用の合法性チェックは各 codegen（と最終的に対象言語のコンパイラ）の責務。
- 「所有を一切使わない」ではない。mutated な decl や、所有が自然な場面では所有を選ぶ。

## 代替案

- **codegen 側で個別に mutation 判定**: 各言語で同じ走査を書くことになり、0001 と同じ理由で却下。
- **自由関数に lifetime `'a` 宣言を足すだけ**: E0261 は消えるが E0507（所有 move）は残る対症療法。
  借用/所有という根の判断をしていない。却下。
- **呼び出し側で一律 `.clone()`**: E0507 は回避できるがコピーコストが乗り、
  zero-copy の意義を損なう。却下。
