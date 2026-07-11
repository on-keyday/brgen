# backend は scalar IO が lowered 済みで届くことに依存してよい

## 日付

- 判断時期: 2026-07-11 (ebm2java の scalar IO フック削除時)
- 文書化: 2026-07-11

## 判断

現行の ebmgen は、複数バイト整数/浮動小数の READ_DATA/WRITE_DATA に
必ず lowered_statement (readBytes + シフト合成の分解形) を付与する。
backend はこの性質に依存してよく、IO ランタイムとして用意すべきは
バイト列の read/write プリミティブだけである。scalar 用の整数合成
ランタイムや read_data_custom の scalar 経路は必須ではない。

## 動機

- **write once generate any language code**: エンディアン処理と
  ビット合成の正しさを ebmgen (transform) が一回だけ担い、backend は
  バイト列 IO という最小の言語固有面だけ書けばよい。新言語立ち上げの
  必須実装がさらに減る。

## 具体例

- ebm2java は当初 ts の DataView フックを模して scalar 用の
  read_data_custom/write_data_custom と readUInt/readInt/writeUInt
  ランタイムを実装したが、全テスト入力 (52 ケース) の生成物で呼び出しが
  0 件であることを確認して削除した。残る IO プリミティブは
  readBytes/readRemaining/writeBytes のみ。
- python/ts の scalar フック (struct.unpack / DataView) は
  `if (lowered_statement()) return pass;` 付きであり、lowered が常に
  存在する現状では同様に発火しない。これらは性能・可読性の最適化として
  高レベル側を横取りする「余地」であり、必須実装ではない。

## これは X を意味しない

- 「ebmgen が永久にこの性質を保証する」ではない。lowering を行わない
  scalar IO を将来 ebmgen が生成した場合、scalar フックを持たない
  backend では default 経路が {{Unimplemented}} マーカーを出し、
  unictest が音を立てて落ちる。静かな誤動作にはならないので、
  その時点でこの ADR を更新し必要な backend に scalar 経路を足す。
- 「scalar フックを書いてはいけない」ではない。stdlib の高速経路
  (go の encoding/binary のような) に置換する最適化は、lowered の
  一段下 (INT_TO_ARRAY/ARRAY_TO_INT) か高レベル側の横取りとして
  引き続き有効な選択肢である。

## 代替案

- 「保険」として未使用の scalar 経路を各 backend に残す。却下理由:
  呼ばれないコードは検証されず、EBM 側の変化で壊れても気づけない。
  依存を明示 (この ADR) して失敗を音の出る側に倒すほうが安全。
