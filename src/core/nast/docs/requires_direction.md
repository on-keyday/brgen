# requires の方向分離と as_is の encode 意味論

draft。2026-08-29 の議論の記録。requires 推論 (`bind/requires.{hpp,cpp}`) を
encode / decode 方向別に分けようとすると、as_is format の encode が入力条件式を
どう解釈するかという言語意味論の未決に突き当たる。その論点と、付随して決まる
union 判別子の規則をまとめる。

**段階 1 は実装済み** (2026-08-29)。Requirements 表は decode / encode の
2 組を持ち、辺の種類ごとの取り込み規則は `requires.cpp` の EdgeKind に
そのまま写した。コーパス測定: 能力持ち owner 20 は分離前と同一 (取りこぼし
なし)、**encode 側に入力能力を要求する format は 0**、state の方向が割れた
format が 4 (ebml ×2 = custom encode fn だけが VarintConfig を読む /
yara ×2)。段階 2 (encode 時の入力条件式の意味論と出力能力語彙) と
判別子分類は未着手のまま。

## 現状

requires 推論は Format / Function 単位の一括で、方向を持たない。語彙は
入力ストリーム能力 (peek / backward / remain / offset) と state 変数の
読み書き。custom encode/decode fn の要求も両方向まとめて format に吸収している
(`requires.cpp` の Collector 末尾)。

## 測定: 主戦場は as_is

`example/` 314 ファイルで、ストリーム能力の要求を持つ owner は 20。

| 文脈 | 数 | 例 |
| --- | --- | --- |
| as_is format | 14 | mptcp / mqtt / udp_test (remain)、mactelnet (peek+remain)、smf ×3 (backward)、ipsec / qr_code (offset)、syslog (peek) |
| custom encode/decode を持つ format | 2 | syslog |
| fn | 4 | syslog の peek ヘルパ、qr_code の offset fn |

custom fn の方向はその fn の所属で自明に決まる。問題は 14 の as_is で、
宣言的 body が両方向の codec を兼ねるため、body 中の `input.remain` 等が
encode 側の要求でもあるのかに答えが要る。

## ebmgen は仕様ソースにならない

ebmgen は as_is body を Encode / Decode の 2 モードで二重変換し、
`input.offset` / `input.remain` はモードに応じて io 参照を付け替える設計に
なっている。ただしその分岐は逆転している (rebrgen
`src/ebmgen/convert/expression.cpp:675-713`):

```cpp
if (ctx.state().get_current_generate_type() != GenerateType::Encode) {
    body.io_ref(current_encdec.encoder_input_def);   // Decode のとき encoder 側 + OUTPUT
    ...
}
else if (ctx.state().get_current_generate_type() != GenerateType::Decode) {
    body.io_ref(current_encdec.decoder_input_def);   // Encode のとき decoder 側 + INPUT
    ...
}
else {
    // Encode かつ Decode はありえないので到達不能
```

つまり「encode 時の `input.remain < 4` が何を意味するか」は実装から逆算できず、
言語仕様として未確定である。逆転分岐は rebrgen 側の別件バグとして扱う
(offset を使う ipsec / qr_code 系が unictest で踏まれているかは未確認)。

## 提案: 2 段階

### 段階 1 — 意味論の決定を最小にした保守的な分離

- Requirements を encode 用 / decode 用の 2 組にする
- custom encode/decode fn の要求はその方向だけに計上し、fn 呼び出しは
  呼び出し文脈の方向へ伝播する
- **as_is body の入力能力 (peek / remain / backward / offset) は decode 側
  だけに計上する**。encode は書くだけで、先読み・残量・巻き戻しは入力の
  性質だから
- state の読み書きは両方向 (as_is の文は両方向で実行される)

93% の要求なし format には影響がない。ebmgen の
`TODO: strictly analyze state variable usage in ast` の per-function 対応は
ここまでで完成する。

### 段階 2 — 言語設計とセットで決めるもの

1. **encode 時の入力条件式の意味論**。`if input.remain < 4:` の encode は
   分岐をどう選ぶか。保持データ由来 (available / presence ベース) に
   読み替えるのが段階 1 の宣言と整合する案
2. **出力能力の語彙**。encode 側にも本質的な能力要求はある — 長さ後書き
   (patching) の out_backward + out_offset、有界バッファの out_remain。
   ただし現状の .bgn には出力を巻き戻す構文が無く、語彙だけ先に作っても
   消費者がいない。`input.offset` の encode 側対応物 (何バイト書いたか =
   out_offset) はこの語彙に属する

## 付随して決まる規則: union 判別子の 2 分類

「encode の分岐選択は保持データ由来」と宣言すると、union の判別子に言語上の
区別が生まれる。

1. **field 由来** — 分岐条件が保持データ (直列化されるフィールド) だけで
   書けている。`match tag:` の tag や、peek して保持したフィールドによる
   match。判別子は条件式から再計算できるので、別のタグ保存は要らない。
   EBM の fold 判別子 (STRUCT_UNION.related_field) が畳んでいるのはこれ
2. **IO 状態由来** — 条件が `input.remain < 4` のような IO 状態を含む。
   decode 時は入力が答えを持つが、encode 時には評価する対象が存在しない。
   よって「どのメンバを保持しているか」は条件から導出できず、**判別条件と
   分離したタグとして実体化しなければならない**

言語別の含意: C++ / Go はメンバ別格納の presence が暗黙のタグになるので
追加コストなし。C の overlay union のようにメモリを重ねる表現では、
IO 状態由来の判別子を持つ union に明示的なタグスロットが必須になる。
Rust の enum はタグ内蔵。

この分類は front end で計算できる: UnionType の各候補の条件式が
StreamType 経由の読みを含むかは、requires の Collector と同じ走査で判定
できる。置き場は UnionLayout 表 (`bind/union_layout.{hpp,cpp}`) に判別子
分類の 1 列を足すのが自然で、タグ実体化が要る union を backend が表から
読むだけになる。

## 未決の列挙

- 段階 1 の宣言「encode は as_is 由来の入力能力を要求しない」の可否
- encode 時の入力条件式の読み替え規則 (段階 2-1)
- 出力能力語彙の命名と、出力巻き戻し構文を言語に入れるか (段階 2-2)
- GenericFormat は requires の owner 収集対象外のまま。instantiation ごとの
  要求は monomorphize の後段
- field 引数の `peek = true` (指示子) を peek 要求に数える件は方向分離と
  独立に入れられる
