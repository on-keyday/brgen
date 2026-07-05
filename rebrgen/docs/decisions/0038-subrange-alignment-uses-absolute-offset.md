# subrange 内のアラインメントは絶対 offset(親 offset 継承)を基準にする

## 日付

2026-06-20

## ステータス

決定(spec)。実装は [[0039-runtime-state-companion-struct]] の lower_runtime_state 経由で
ebm2rust(std-io/zero-copy)・ebm2go(std/slice/append)が `subrange_align_test` pass 済み(2026-07-05)。
他 backend(ts/cpp/ruby/rmw/c/zig 等)は未実装で fail のまま — 引き続きトラッカーとして追跡。

## 判断

`input.align`(および `input.offset` / GET_STREAM_OFFSET)が参照する offset は、
`input.subrange(...)` で作った子ストリームでも **親ストリームの offset を継承した絶対値**とする。
子ストリームで 0 にリセットしない。

## 動機

- きっかけは次の最小エッジケース(`src/test/subrange_align.bgn`):
  ```
  format EdgeCase:
      len :u16
      inner :Inner(input = input.subrange(len))
  format Inner:
      align :[..]u8(input.align = 32)
  ```
  `Inner` は align のみなので、padding 量は **subrange 境界での offset ゼロ点ポリシー**だけで決まる:
  - 絶対(親 offset 継承): Inner は外側 offset 2(u16 の後)→ `(4 - (2 & 3)) & 3 = 2` バイト padding
  - subrange ローカル(0 リセット): offset 0 → padding 0
- 実バイト列上、Inner のバイトは外側 offset 2 に位置する。「32bit 境界に揃える」は**転送ストリーム上の位置**を意味するのが自然で、subrange が偶々非アライン境界から始まっても正しく padding されるべき。netlink 等の「メッセージ先頭基準アライン」とも整合。
- IR フラグ名が `has_absolute_offset` であること自体、絶対を意図していたことを示唆する。
- この論点は本 ADR 以前に `web/doc/content/docs/todo.md`(2025/11/04 時点)で既出:
  「subrange における 0 を示す? <-これは微妙だ。これだと全入力の任意の位置は指し示せない」
  「全体の入力の offset <-こっちが良いと思われる」。本 ADR はこの当時の傾向と同じ結論を、
  エッジケース(`subrange_align.bgn`)による具体的根拠付きで確定させたもの。

## 具体例 / 現状

- `subrange_align_test`(入力 `0002 0000` = len 2 + padding 2B)は絶対セマンティクスでのみ round-trip する。
- 現状はどの backend も未対応で fail:
  - ebm2rust: GET_STREAM_OFFSET 自体が未実装(`{{Unimplemented}}`)
  - ebm2go: subrange 呼び出しが align 用 `absOffset` companion を渡し忘れ → 引数個数不一致でコンパイル不能
  - ebm2ts: 子 `{view, offset:0}` にリセット → subrange ローカルになり padding 0(本 ADR と不一致)
  - ebm2rmw: `PUSH_SUB_INPUT: length is not integer`(subrange 長の型まわりの別制限も露出)

## これは X を意味しない

- offset の**実装方式**(companion state を threading するか / reader-with-offset struct か / streaming を維持するか / bit 粒度)はここでは決めない。それらは別途。本 ADR は「subrange 境界で offset を継承する(絶対)」という**ゼロ点ポリシーのみ**を固定する。
- 「subrange は新ストリーム」という見方を全否定するものではない。読み書きの座標(バッファ index / cursor)は subrange ごとに 0 から始まってよい。**アラインメント基準の offset だけ**は親から継承する、という分離(go が `absOffset` を read cursor と分離しているのと同じ方向)。

## 代替案

- **subrange ローカル(0 リセット)**: 実装は単純だが、Inner=align のみのとき padding が常に 0 になり align が実質無効化される。非アライン境界から始まる subrange で「転送ストリーム基準のアライン」を表現できない。却下。
- **DSL で明示**(`input.subrange(len, keep_offset=false)` 等で都度選ばせる): 表現力は最大だが、大多数のケースで絶対が正なら冗長。まず絶対をデフォルトとし、必要が出たら拡張する(ground before remodel)。
