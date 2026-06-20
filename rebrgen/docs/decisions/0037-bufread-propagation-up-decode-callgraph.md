# decode の BufRead 要求を呼び出しグラフ越しに親へ伝播する（ebm2rust）

## 日付

2026-06-20

## 判断

ebm2rust で、ある decode 関数が直接 `fill_buf`(until-EOF 終端判定)を使う場合だけでなく、
**`fill_buf` を要する子 decode を呼ぶ親 decode** も decoder 入力を `&mut impl std::io::Read`
ではなく `&mut impl std::io::BufRead` で生成する。`Statement_FUNCTION_DECL_before` の
`DefUseCollector` に `Context_Expression_CALL` の訪問を追加し、callee の `HasFillBuf`
マーカーを見て自分にも伝播させる。

## 動機

- `enough to represent formats` / 多言語生成の正しさ。ipv6.bgn の `NDPNeighborSolicitation`
  は `options: [..]NDPOption`(until-EOF 配列)を持ち、その decode は終端判定に `fill_buf` を
  使うため `&mut impl BufRead` を要求する。一方それを含む `TestPacket.decode` は直接 `fill_buf`
  を使わないため従来は `&mut impl Read` で生成され、`self.ndp.decode(reader)` に `impl Read` を
  渡して **E0277 `impl Read: BufRead` not satisfied** になっていた。
- 「全 decode を一律 BufRead にする」案は却下(下記)。必要な関数だけ昇格させ blast radius を
  最小化する。

## 具体例

- `NDPNeighborSolicitation.decode(&mut impl BufRead)`(直接 `fill_buf`)
- → `TestPacket.decode` は `ndp.decode` を呼ぶので `&mut impl BufRead` に昇格(伝播)
- 他の decode(EthernetFrame / IPv6Header 等)は `&mut impl Read` のまま
- マーカーは `function_markers`(statement id → FunctionFlags)で管理。callee の解決は
  `ctx.get_field<"member.body.id">(call_desc.callee)` で WeakStatementRef を得て id を引く。

## これは X を意味しない

- **全 decode を BufRead にするのではない**。読むだけ/peek 不要の decode は `impl Read` のまま。
  `impl Read` の方が呼び出し側の制約が緩く(任意の Reader を渡せる)、不要な BufRead 要求は
  API を狭める。必要な関数だけ昇格する。
- **完全なデータフロー解析ではない**。伝播は「callee は親より先に emit される(フォーマット
  定義順がボトムアップ)」前提で、親の before-hook 時点で callee マーカーが確定していることに
  依存する。brgen は定義前参照を許さない(値埋め込みは定義済み型のみ)ため実フォーマットでは
  成立するが、相互再帰のような前方参照が将来入る場合は call graph の不動点計算へ拡張が必要。

## 代替案

- **全 decode を `impl BufRead`**: 1 行で E0277 は消えるが、全フォーマットの decode シグネチャを
  変え、peek 不要の decode にまで BufRead を要求して API を不必要に狭める。却下。
- **until-EOF 終端判定を BufRead 非依存にする**(1 バイト読んで EOF を例外で判定等): 終端と
  途中切り詰めの区別が曖昧になり、`fill_buf` による「残データ有無」のクリーンな判定を失う。却下。
- **call graph の不動点計算でマーカーを事前一括計算**: 前方参照にも頑健だが、現状の実フォーマット
  では in-order 伝播で十分。必要になった時点で拡張する(ground before remodel)。
