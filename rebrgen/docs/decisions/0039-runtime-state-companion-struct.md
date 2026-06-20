# runtime state を gated な専用 companion struct で持つ

## 日付

2026-06-20

## ステータス

決定(方針)。実装は未着手。実装ターゲット = `native_endian_test`(`input.align`)+ `subrange_align_test`(ADR 0038)。

## 判断

decode/encode が実行時に保持する必要のある合成 state(まず stream offset。byte/bit）を、
**コンパイラ管理の専用 companion struct(`RuntimeState`)に束ねて持つ**。

- **必要な時だけ threading し、不要なら完全に省略**する(= 今までのシグネチャを維持)。
  gate は「runtime state が要るか」フラグで、既存の `has_absolute_offset`(io_input_desc)の一般化。
- **reader は RuntimeState に含めない**(含めると全 decode が必須になり省略できなくなる)。
  reader は今まで通り独立の io param、RuntimeState はその横に並ぶ純粋な追加 state バッグ。
- **user `state`(DSL の `state`）とは混ぜない**。GET_STREAM_OFFSET 等は `RuntimeState` への
  MEMBER_ACCESS に lowering する。

## 動機

- brgen が「実行時に保持すべき情報」を持つケースが増えている/既にいくつかある:
  stream offset(byte)、bit offset(`input.bit_offset` / 非バイト境界 align)、将来の runtime info。
  現状これらは各 backend が**個別の companion を各自再発明**していて、しかも**食い違い・バグ**がある:
  - ebm2go: `absOffset` companion(`*int`)を threading するが、**subrange 呼び出しで渡し忘れ**
    → 引数個数不一致でコンパイル不能(`subrange_align_test`)。
  - ebm2rust: std-io は offset を持たず GET_STREAM_OFFSET 自体が未実装。direct のみ `<io>_off`。
  - ebm2ts/cpp: reader オブジェクトが intrinsic に offset を持つ(`{view,offset}` / futils reader)。
  - ebm2rmw: VM stream object。
  → 「offset を持つ」という1次情報を IR 側で1つの gated companion に集約し、threading/伝播/lowering を
    write-once にする。新しい runtime 需要は **RuntimeState のフィールド追加だけ**で済み threading は不変。
- user `state` を巻き込まない理由: user state は DSL に実体があり、どう小分けするかは**ユーザの領分**。
  runtime state と同じ箱に入れると、本来引き継がない scope にまで user state が**過剰伝播**する。
- endian を含めない理由: endian は convert-time の lexical scope(`set_endian`）で解決され、
  runtime に持つのは dynamic の時だけ。本 ADR の「実行時保持」カテゴリとは別機構。

## 具体例 / 実装方針

- **gate と伝播**: 関数が直接 offset を使う、または offset を要する子 decode を呼ぶ場合に RuntimeState を
  threading する。後者は **呼び出しグラフ上方への伝播**で、[[0037-bufread-propagation-up-decode-callgraph]]
  の `HasFillBuf` マーカー伝播と同じパターン(子のマーカーを見て親にも立てる)。
- **subrange/ネスト**: 子が RuntimeState を要する時だけ子 RuntimeState を構築。offset は親から継承
  (= [[0038-subrange-alignment-uses-absolute-offset]] の絶対 offset)。読み書きカーソル(buffer index /
  cursor)は subrange ごとに 0 でよいが、**alignment 基準の offset だけは親から seed**。
- **増分**: RuntimeState.offset は READ_DATA/WRITE_DATA 時に `+= size` で更新(size は IR が保持)。
  これが endian(不変・convert-time)と違う唯一の新規ロジックで、IO 文に局所化される。
- **realization knob**: bare reader(rust std-io `impl Read` / go `io.Reader`)は RuntimeState を struct/
  companion として実体化。intrinsic offset を持つ backend(ts `{view,offset}` / cpp futils reader）は
  RuntimeState.offset を native reader にマップして synthetic offset を opt-out できる。
- **streaming 維持**: RuntimeState は reader を内包しないので、逐次読み(`impl Read` / `io.LimitReader`)を
  維持したまま offset companion を併走できる(全バッファ化に倒さない)。

## これは X を意味しない

- **user state を struct に巻き込むのではない**。user `state` の既存 per-var threading(`ast_field`
  マッチ)は温存。RuntimeState はコンパイラ合成の別物。
- **reader を struct に入れるのではない**。省略可能性を保つため reader は独立 param のまま。
- **常に存在するのではない**。offset 等を使わない大多数のフォーマットでは RuntimeState は出ず、
  シグネチャは現状と完全に同じ。
- **struct か単一 companion かは realization**。フィールドが offset 1つの間は実質 `*int`/`&mut usize`
  companion と同じ。複数(offset + bit + 将来)になった時に struct の価値が出る。

## 代替案

- **per-concern companion を各 backend で個別実装(現状)**: go の `absOffset`・rust の `<io>_off` 等が
  重複し、伝播配線を各自書いて各自バグる(go の subrange 渡し忘れ)。集約で解消。却下。
- **user state と同じ箱に入れる**: user state の過剰伝播を招く。却下。
- **reader を RuntimeState に内包(coder オブジェクト化)**: subrange の子構築は綺麗になるが、
  全 decode が RuntimeState 必須になり「不要時省略」を満たせない。却下。
- **全入力をバッファ化して intrinsic offset に統一(reader-object 一本化)**: GET_STREAM_OFFSET は
  一律 `<io>.offset` になるが streaming decode を捨てることになる。streaming は原則維持の方針に反する。却下。
