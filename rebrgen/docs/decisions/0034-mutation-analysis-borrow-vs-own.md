# mutation analysis で借用 / 所有を決定する（is_mutable の一般化）

## 日付

- 判断時期: 2026-06-18
- 文書化: 2026-06-18
- ステータス: 採用（実装済。2026-06-20 に下記「補足」を追記）

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

## 補足: 借用条件は「mutated でない」だけでは不十分（2026-06-20）

実装後、ebm2rust で 30 件のコンパイルエラー（Rust E0308 mismatched types:
`&[T]` を `Vec<T>` / `Cow<'a,[T]>` フィールドへ代入）が発生した。原因は借用可否の
判定が粗すぎたこと。**param が読むだけ（pass-through / index など）なら借用できるが、
owned 領域（struct フィールド等）へ store（move）される param は所有が必須**である。
`is_mutated`（= 代入 target として書き換えられるか）だけでは後者を捕捉できない。

具体的には、合成 setter が `self.<field> = param` で param を owned フィールドへ
書き込むケース:

- `transform/array_setter.cpp` の `VECTOR_SETTER`（`self.data = param`）
- `transform/derive_property.cpp` の `PROPERTY_SETTER` / composite setter
  （`self.<field> = (cast)arg`、variant メンバへの書き込み含む）

これらの代入は **convert 後の transform で合成される**ため、convert 時の mutation
解析（代入 target の root を mutated マークする処理）の射程外であり、param は
`is_mutated=0` のまま借用されてしまう。対策として、両 transform で param 生成時に
`is_mutated(true)` を明示マークし、所有を強制する（store する param は所有必須、という
1 次情報の補完）。これにより param 宣言（`param_visitor`）と呼び出し側
（`as_arg_visitor`）が同じ `is_mutated` を見て一貫して所有を選ぶ。

注: `bit_holder.cpp` の setter param はビットサブフィールド（uint）で、借用は VECTOR
限定のため影響しない。また `isUTF8(data :[]u8)` のような読むだけの自由関数 param は
引き続き `&[u8]` で借用される（本 ADR 本来の最適化は維持）。
