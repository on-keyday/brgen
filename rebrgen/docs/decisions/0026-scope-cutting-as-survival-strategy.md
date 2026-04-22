# スキーマ記述言語における「何を切るか」の判断

## 日付

2026-04-19

## 判断

バイナリフォーマット記述言語というニッチにおいて、「何を全部やろうとするか」ではなく「何を切るか」こそがプロジェクト生存の条件である。brgen/rebrgen は以下の切り方を選んでいる:

- **切った軸**: writer 側の整合性自動修復(checksum/length 等の generative value を「自動で直す」こと)、declarative 単一モデルへの固執
- **残した軸**: encode/decode 両対応、整合性 **検証**、format 記述の declarative コア
- **追加した escape hatch**: user-defined state variable、`input.align` / `input.bit_offset`、byte IO と(計画中の)very_slow_bit_ops の dual 実装

この「線引き」を後世に共有可能な設計判断として明示する。

## 動機

- **先行事例の観察**: mjt(okuoku)氏の yuni(2009年、binary format 記述言語として構想、後に Scheme portability library へ転生)、DFDL 等、「declarative + encode/decode 両対応 + 任意 wire format + 自動整合性」という全部盛りを目指したものの多くは未完・研究止まり・scope inflation で埋没した経緯がある。
- **Kaitai Struct の生存要因**: encode を明示的に切り、decode + declarative に絞った。リバースエンジニアリング・フォレンジック・不明 format の観察という明確な user segment に刺さって 10 年以上 active。
- **ASN.1 の生存要因**: 「ユーザーが任意の wire format を指定する自由」を切り、BER/DER/PER/OER/JER 等の encoding rule が wire 表現を決める形に限定。その代わり type definition の declarative 性と encode/decode 両対応を維持。X.509、LDAP、SNMP、3GPP 系プロトコル等で現役。brgen/rebrgen が「任意 wire format の記述」を目的とする点とは切り方が真逆だが、**どちらも「何かを切った結果として生存している」**点で構造的に同じ。
- **「切る判断」の構造**: read/write 対称性を declarative で自動導出することは DSL のコア価値だが、同時に encode 側の整合性(checksum/length/CRC 等の "generative value" 問題、mjt 2009 記事 c 節)を抱え込むと scope inflation で必ず詰む。**「全部やる」と「declarative で綺麗に閉じる」は二択**。
- **user segment 駆動**: rebrgen は実 format(3GPP MIB、SMPTE、QUIC 等)を user 自身が .bgn で書き、unictest で殴って回している。10% の変態ケースが「レア」ではなく「即日当たる壁」として現れるため、procedural escape を認める判断が自然に出る。

## 具体例

**rebrgen が切った axis の具体:**

1. **writer 側の整合性自動修復は捨てた**
   - checksum/CRC/複雑な length 依存は自動では直さない
   - 単純ケースのみ auto-setter 的に補助
   - `@assert` 等で **検証** はするが、通らなかったら error を返して user に整合させる責任を返す
   - 結果: 「全自動双方向導出」は諦めたが、仕様が肥大化せず挙動が予測可能

2. **declarative 単一モデルへの固執は捨てた**
   - state variable: 手続き的 state を declarative 記述の中に取り込む(ADR 0008 等と関連)
   - `input.align` / `input.bit_offset`: 低レイヤ IO 位置を明示的に触らせる
   - dual byte-IO / very_slow_bit_ops: 「全 format を bit-stream 一本化」ではなく、fast path は fast のまま保ち、bit-unaligned case 用の別実装を並行して持つ(`user_value_practical_performance` 相当の判断)

3. **「encode/decode 両対応」は切らなかった**
   - Kaitai と違ってここは譲らない。decode 専用では brgen が想定する「実 format を双方向で扱える generator」のコアが成立しない

**残した declarative コア:**
- format 記述の構文は宣言的
- 可視化(ebm2mermaid/graphviz 等)、静的解析(unictest、型チェック)、逆方向導出の一部(decode の自動化、単純ケースの encode)は declarative 層の恩恵として得ている
- procedural escape はあくまで「宣言的では書きにくい所に穴を開ける」立ち位置で、主役ではない

## これは X を意味しない

- **「declarative を捨てた」ではない**: 主幹は依然として declarative。procedural escape は限定的な穴。
- **「Kaitai / ASN.1 より優れている」という主張ではない**: user segment と切る axis が違う。いずれも異なる axis を切った結果として生存しており、それぞれ別解として両立する。rebrgen は「任意 wire format の記述 + encode/decode 両対応」を守る代わりに整合性自動修復と declarative 単一性を切った。ASN.1 は任意 wire format を切って encoding rule 固定に。Kaitai は encode を切って decode 特化に。**切り方が違うだけで、生存戦略としては同型**。
- **「先人は技術的に劣っていた」ではない**: 先人たちの挫折の大半は技術力不足ではなく、scope を絞る判断ができない環境(学術 publication、bidirectional を売りにした時点での構造的必然)に起因する。
- **「切った軸は永遠に切ったまま」ではない**: 遠い将来、先行事例の積み重ねから「全部自動化」が成立する可能性は排除しない。現段階で切っているだけ。scope 切りの判断自体を ADR として残す理由は、将来の再統合の際に「なぜ一度切ったか」が失われないようにするため。

## 代替案

- **encode を切る(Kaitai 路線)**: 検討に値するが、rebrgen の想定 user segment(実 format の双方向実装が要るエンジニア)と合わない。不採用。
- **整合性を完全自動修復する(全部自動双方向)**: 先人の挫折パターン。scope inflation で確実に詰む。不採用。
- **declarative を単一モデルで通す(procedural escape なし)**: bit-unaligned entry、state を持つ format、復号依存の length 等が書けなくなる。不採用。
- **切る判断を ADR 化せず実装だけ進める**: 今後誰か(user 自身含む)が「なぜこんな設計にした」と訝しがった時に、意図が復元できなくなる恐れ。却下、本 ADR を書く理由そのもの。

## 関連

- ADR 0021(positioning-among-idl-tools): 既存ツールとの位置関係の事実整理。本 ADR はその上位の「なぜそう位置取ったか」の設計哲学。
- ADR 0022(few-runtime-dependency): few runtime も「切る判断」の一例。
- ADR 0023(ecosystem-deprioritized): 「velocity over adoption」との接続。
- 先行事例の調査メモは本 ADR 作成時の会話に残存(mjt hatenadiary 2009-10-17、propella 2009-10-16、Kaitai Struct、yuni → Scheme portability への転生等)。
