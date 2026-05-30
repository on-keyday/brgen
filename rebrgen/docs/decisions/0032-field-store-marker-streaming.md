# FIELD_STORE marker で decode の field store を抽象化し streaming / Sans-I/O codegen を導出する

## 日付

- 判断時期: 2026-05-29（FIELD_STORE marker / convert producer）、2026-05-30（token passthrough / round-trip harness）
- 文書化: 2026-05-29、追補 2026-05-30

## 判断

decode 時に「読んだ値を出力 field へ格納する」操作を、EBM の marker statement
`FIELD_STORE`（`FieldStoreDesc{ field, target, source, lowered_statement }`）で
包む。default 挙動は `lowered_statement`（通常の格納 statement）を visit するだけ
なので DOM 型 generator（C/Go/Rust/...）は無変更・出力バイト不変。streaming 型
generator（初出は ebm2wuffs）は `field_store_custom` を override し、field を
materialize する代わりに値を sink へ **emit** する。

marker は EBM 上に常時存在させ gating しない。挙動の差は generator 側だけに置く。
これは既存の `INT_TO_ARRAY` / `ARRAY_TO_INT`（endian 変換 marker）と同じパターンの
踏襲である。

## 動機

- **write once generate any language code**: Wuffs は no-heap かつ Sans I/O
  （ライブラリは I/O を持たず、バイト供給と出力受け取りは caller 責務）の codec
  言語であり、可変長データを struct field に保持できない。EBM の現行モデル
  「format = 全 decode 値を所有する struct + `decode(src)` が `this.*` を埋める」
  （C/Go/Rust 等に綺麗に写る DOM モデル）は、可変長・variant を持つ format で
  Wuffs に写らない。同じ EBM から「格納せず流す」派生を導出できれば、Sans-I/O 系
  backend を同一 IR で賄える。
- 以前から「EBM から流れる系（streaming）モデルを導出したい」という設計欲求が
  あり、ebm2wuffs がその具体的トリガーになった。
- 再利用性: Wuffs 専用ではない。SAX / pull parser / 低メモリ C パーサ等、
  「結果を所有せず逐次出力する」任意の backend が同じ marker を消費できる。

## 具体例

- marker descriptor: `FieldStoreDesc{ field:WeakStatementRef, target:ExpressionRef,
  source:ExpressionRef, lowered_statement:LoweredStatementRef }`。
- default hook `Statement_FIELD_STORE_class.hpp`:
  `field_store_custom` があれば呼ぶ（CALL_OR_PASS）。無ければ
  `ctx.visit(ctx.field_store.lowered_statement.id)` ＝ 通常 DOM 格納。
- scaffold 段階（producer 未実装）では FIELD_STORE ノードは一切生成されないため、
  既存 generator の出力はバイト不変であることを確認済み（ebm2wuffs / ebm2c で
  leb128 を生成、exit 0・サイズ不変）。
- ebm2wuffs（実装済み）: streaming を **token passthrough** として具体化した。
  decode は全 READ_DATA が「消費する src バイト範囲」を field タグ付き token
  （`write_simple_token_fast`, value_major=0xEB23F, value_minor=field id,
  length=消費バイト数）として `base.token_writer` へ流す。可変長 field は struct
  から省略し（その field を参照する setter/getter/length-check/encode も省略 or
  honest stub 化）、scalar も struct 格納と並行して token を吐く。vector 要素は
  scalar read に lower されるため非 byte vector も同経路で covered。
- decode round-trip 検証: `ebm2wuffs/unictest.py` が wuffs-gen 済み C を compile し、
  INPUT を decode coroutine に流し、emit された token の length 列から src を再構成
  して INPUT と byte 比較する。byte-vector-only の until_eof と scalar+u16-vector の
  simple_vector が round-trip 通過（exit 0 → framework が OUTPUT==INPUT を確認）。

## これは X を意味しない

- **EBM が Sans I/O になる、ではない。** EBM は依然「caller 供給の *完結した
  random-access バッファ* に対する依存性注入 I/O」である（underflow は terminal
  error で suspend/resume を持たない。`is_seekable` / `ReserveData` は random
  access 前提）。FIELD_STORE が扱うのは「格納 vs 放出」の軸だけ。Sans-I/O 完全化に
  必要な他2軸は別物:
  - 読み側 suspend-on-underflow: Wuffs ターゲットでは `?` から Wuffs compiler が
    coroutine 状態を合成するため EBM の課題ではない。native coroutine を持たない
    言語 × incremental 入力を狙うときだけ EBM 側に suspend 概念が要る。
  - 書き側 back-patch: EBM の encode は明示長 field + `LENGTH_CHECK` assert で
    既に forward-only が支配的（`ReserveData` は seekable 時のみの省略可能経路）。
- **marker 単体で streaming が閉じる、ではない。** decode 内で再参照される field
  （例: leb128 の `len`/`value` がシフト量・累算に使われる = P_intra）は、store を
  emit に替えるだけだと後続の member read が壊れる。streaming 側で
  **field → decode-local 昇格**（read も local に解決）と、loop で更新される field の
  **emit 配置を「最後に到達する def の後（loop 外）」に置く**処理（def-use/liveness
  解析）が別途必要。これらは struct 形状と read site を変えるため DOM へ波及させ
  られず、**streaming generator 側（ebm2wuffs）に置く**。
- **DOM generator の挙動を変える、ではない。** lowered_statement fallback により
  C/Go/Rust 等は無変更（scaffold は inert と検証済み）。
- ここでの「streaming」は emit-not-store の意味であり、coroutine/suspend の意味
  ではない（後者はターゲット言語の関心事）。
- **token が値を運ぶ、ではない。** token は field タグ（value_minor）と消費バイト長
  （length）のみを持ち、値そのものは src バッファ参照で表す（SAX のイベント境界に
  近い）。再構成は token length 分を src からコピーするだけで、エンディアンや bit
  解釈は介在しない（raw byte 透過）。
- **全 format が round-trip する、ではない。** decode 側の token 化自体は scalar /
  byte vector / 固定 byte array / bit field を網羅した（bit field は「backing byte を
  byte 単位 read → 各 byte に token → 読んだ値から shift/mask 抽出」と lower される
  ため、追加対応なしで token 化済み）。ipv6 が round-trip しない原因は **token 化の
  不足ではなく、生成 Wuffs が C にコンパイルできないこと**である（下記2項）。
- **`wuffs gen` 成功は C compile 成功を意味しない。** `wuffs gen`（型検査＋bounds
  prover）が通っても、emit された C が標準 Wuffs ランタイムに対してコンパイル/リンク
  できるとは限らない。実測で 2 つのギャップを確認: (1) `io_writer.write_u16be?` は
  builtin 宣言があり型検査を通るが、`wuffs_base__io_writer__write_u16be` の C 実体が
  ランタイムに無い（std のどこも使っておらず base 実装ソースにも無い。`write_u8?`
  だけは compile 可）。(2) 同一パッケージの sub-struct を struct field に埋め込むと
  init が `self->private_data.f_X` を生成するが、その struct に `private_data` が宣言
  されずコンパイルが落ちる。**Wuffs std は同一パッケージ struct の field 埋め込みを
  一度も使っておらず**、埋め込むのは常に別パッケージの decoder。composite format
  （ipv6/tcp/dns 等）はこの (2) が壁。token 化が済んでいても round-trip は未完成。
- **encode は全 struct で honest stub。** 格納しない field は encode で復元できず、
  かつ上記 (1) により多バイト整数を書く encode はそもそも C にリンクできない。よって
  vector 有無に関わらず全 ENCODE を `return "#error"` に stub する。encode 側
  streaming は別課題。
- **failure-case の PASS は exit code 設計で誠実化済み。** unictest framework は
  runner の exit `CONFIRMED_DECODER_FAILURE_EXIT_CODE`(=10) のみを「decode が
  期待通り失敗」と解釈し、他の非ゼロは "test internal error"=FAIL とする。当初 runner は
  `wuffs gen` 失敗にも exit 10 を割り当てていたため、format の `.wuffs` が gen 落ちする
  と invalid/truncated 入力も同じ gen 落ちで spurious に PASS していた。runner で gen
  fail を exit 1（internal error）に変更したのでこの spurious 経路は閉じ、PASS 数は
  真の round-trip 数に一致する。現状 PASS=3（simple_vector / simple_vector_empty /
  until_eof）。
- **配置制約（意味論が第一、proof は整合）。** read は emit より先に置く。第一の
  理由は意味論である: token は「実際に消費した src バイト」の記録なので、read が
  失敗しうるのに emit だけ先に出ると「読めていないのに token がある」矛盾した stream
  になる。read 成功が emit の前提であり、これは prover とは独立に成り立つ正しさの
  要件である。
  これと整合する形で proof 上の制約もある: dst-space guard と token write の間に
  fallible（`?`）call を挟めない。read は `?` coroutine call で、suspend を跨ぐと
  prover が `args.dst` の事実を落とすため、guard が立てた `dst.length() >= 1` と
  write の間に read が入ると前提が消える。両者を同時に満たす配置が
  `read? → (guard + write)` で、read を先に置きつつ guard と write を隣接させる。
  guard 自体は単発 `if/yield` ではなく `while/continue` の 1-shot loop（fall-through
  では write 直前に下界が再確立されない；byte vector loop と同型）。

## 代替案

- **「streaming-lowered EBM」を別 IR variant として gating 生成。** 却下: 
  INT_TO_ARRAY パターン（普遍 marker + lowered fallback + generator 別 dispatch）の
  ほうが綺麗で、gating 不要・DOM 無傷・IR を marker で enrich するだけで済む。
- **marker 無しで ebm2wuffs が自前で field store を検出。** 却下: 共有・再利用可能な
  primitive を失い、各 streaming backend が同じ解析を再導出することになる。marker は
  将来 def-use/emit 配置情報を付与する自然な場所にもなる。
- **field→local 昇格 + emit を普遍 transform にする。** 却下: struct 形状と read site
  を変えるため DOM generator を壊す。streaming ターゲット側に留めるべき。

## 関連

- ADR 0031（pure VARIANT 型の declaration 生成は backend 責務）: 「表現は backend が
  決める」哲学の同系。variant も streaming 下では「格納せず分岐して emit」に解消され、
  両判断は親和的。
- ADR 0005（ebmcg / ebmip 分離）: streaming は CodeWriter 中心の default visitor から
  外れる面があり、将来 ebm2wuffs を ebmip 寄りに再配置する論点に繋がりうる（現状は
  ebmcg）。
- ADR 0017（transform passes）: Phase 2 の producer は最終的に **convert 側
  (`statement.cpp` の代入/APPEND 変換)** に実装した。当初 transform pass で後付け
  wrapping を試みたが、target の `this.field` を get_field で再発見する方式が複雑
  フォーマット(leb128/DNS label)で脆く crash した。convert は生成時点で field /
  target / value が手元にあり、`has_self` + field-decl 解決だけで堅牢に判定できる。
  decode-output の field store は decode.cpp の machinery と、ユーザが `fn decode()`
  に手書きした代入(leb128 の `value |= ...`)の2系統から来るため、convert 側でも
  単一チョークポイントは無く、現状はユーザ手書き代入(statement.cpp)を wrap する。
  machinery 側(int/enum/array の暗黙 decode)の wrap は今後の課題。
  検証は unictest round-trip(ID 名に非依存)で行い、byte-diff は不適(生成識別子が
  EBM ID を埋め込むため producer の ID 採番でノイズ爆発)と判明した。
