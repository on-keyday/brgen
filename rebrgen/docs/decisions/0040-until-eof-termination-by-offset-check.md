# until-eof の終端判定は EOF 分類 + offset 照合で厳密化する(lookahead 不要)

## 日付

2026-07-06

## ステータス

決定(方針)。実装は未着手。実装ターゲット = ebm2zig の `native_endian_test`
(現状 `can_read_stream_visitor` が `true` 固定 → until-eof ループが無限化し
EndOfStream で fail)と ebm2go の `ipv6_invalid_test/std-io`(現状 EOF-catch
で不正入力を受理する側に倒れている)。

## 判断

until-eof 型ループ(`CAN_READ_STREAM` を終了条件とするループ)の終端判定を、
**「要素 decode を試行し、EOF 系エラーかつ RuntimeState.offset が反復開始時と
一致(= 1 バイトも消費せず EOF)なら正常終了、それ以外のエラーは伝播」**で行う。

```
loop:
    start := runtime_state.offset
    要素を temp に decode してみる
      成功                      → append して続行
      EOF 系 && offset == start → 要素境界ちょうどで入力が尽きた = 正常終了(break)
      EOF 系 && offset >  start → 要素の途中で切れた = 不正入力(エラー伝播)
      その他エラー              → 伝播
```

lookahead(peek/BufRead/バッファ化)は until-eof の終端判定には**要求しない**。
reader は素の streaming インターフェース(`io.Reader` / `anytype` / `impl Read`)のままでよい。

## 動機

- **write once generate any language code / streaming 原則維持**: until-eof の終端を
  lookahead で判定する方式(peek reader / BufRead 要求)は、reader 側に能力を要求し、
  backend ごとに「reader を包む」インフラを要求する。テストハーネスがバッファを
  渡している事実を仕様と混同して「全バッファ前提」に倒すのは論外(ハーネスは
  ユースケースの一つにすぎない)。
- 親が「子がどこで失敗したか」を厳密に知れるなら lookahead は本来不要 — そして
  [[0039-runtime-state-companion-struct]] がまさにその「厳密な消費位置」
  (`runtime_state.offset`)を全アクティブ backend に通した。増分は各 backend の
  READ/WRITE visitor に実装済みなので、この判定に必要な土台は既にある。
- エラー値の分類**だけ**では不十分な根拠: go の `ReadFull` は「0 バイトで EOF なら
  `io.EOF`、部分読みなら `ErrUnexpectedEOF`」を区別するが、**複数 read からなる要素の
  2 番目以降の read が 0 バイト EOF になるケース**(= 要素途中の切断)では `io.EOF` が
  返り境界 EOF と区別できない。offset 照合はこのケースを正しく不正入力として弾く。
- 現状の被害:
  - ebm2zig: `can_read_stream_visitor` が `true` 固定 → `while (true)` で読み続けて
    subrange を突き破り `error.EndOfStream`(native_endian_test fail の直接原因)。
  - ebm2go: EOF-catch(本 ADR 以前の暫定 = 下記代替案 B)で終端しており、途中 EOF と
    境界 EOF を区別できず **invalid 系テストで不正入力を受理**(ipv6_invalid/std-io fail)。

## 具体例 / 実装方針

- **gate**: until-eof ループを含む関数は RuntimeState が必要。convert で
  `CAN_READ_STREAM` を生成する箇所で `has_absolute_offset(true)` を立てる
  (`get_alignment_requirement` と同じ 1 行パターン)。以降の threading / wrapper /
  伝播は [[0039-runtime-state-companion-struct]] の lower_runtime_state がそのまま担う。
- **ループ変換は per-backend**(増分と同じ扱い。ADR 0008 の理由に準ずる): 条件が
  `CAN_READ_STREAM` のループを、各 backend が自言語のエラー処理イディオムで
  「try + EOF 分類 + offset 照合」に展開する。
- **EOF 系エラーの分類**(backend 別):
  - zig: `error.EndOfStream`
  - rust: `io::ErrorKind::UnexpectedEof`
  - go: `io.EOF` / `io.ErrUnexpectedEOF`
  - python/ruby: 例外型(struct.error / EOFError 等、実装時に確定)
  - **ts**: 現在 `new Error("unexpected EOF...")` の文字列 → 型付きエラー
    (例: `class EofError extends Error`)の導入が前提作業
  - **c**: 現在一律 `return -1` → EOF 専用の戻り値コード(例: `-2`)の導入が前提作業
- **副作用封じ込め**: 要素は temp 変数に decode し成功時のみ append する現行の
  生成パターンが、失敗試行の巻き戻し不要性を既に保証している(self を汚す前に
  break で捨てられる)。
- **残る lookahead 需要**: ループ終端**ではない**裸の remaining 比較
  (DSL ユーザーが式として書く `input.remaining >= n` 等)は try-and-check で
  代替できないため、そこには引き続き lookahead/seekable 能力(rust の
  [[0037-bufread-propagation-up-decode-callgraph]] 相当)を要求してよい。
  本 ADR は until-eof 終端のみを対象とする。

## これは X を意味しない

- **ADR 0037(HasFillBuf/BufRead)の撤回ではない**。rust の BufRead 経路は動いており
  急いで置換しない。本 ADR 実装後は「until-eof 終端」用途に限り BufRead 要求を
  外せる余地が生まれる、という関係。lookahead は「remaining を式として知る」用途に残る。
- **EOF-catch(代替案 B)の追認ではない**。offset 照合のない EOF-catch は不正入力を
  受理する側に倒れる。go の現行実装は本 ADR で置換されるべき暫定。
- **ループ変換の IR 化ではない**。展開形(try/catch イディオム)は言語依存なので
  増分([[0008-update-offset-rejected]])と同じく backend 側に置く。IR が保証するのは
  gate(RuntimeState の存在)と offset の正確さまで。
- **reader インターフェースへの新要求ではない**。むしろ要求を減らす方向
  (素の streaming reader で until-eof が厳密に動く)。

## 代替案

- **A: 全バッファ前提化(io を FixedBufferStream 等の既知バッファ型に固定)**:
  can_read/remaining が正確な式になるが、テストハーネスの実装事情を仕様に昇格させる
  ことになり、streaming 原則(および anytype reader の意図)に反する。却下。
- **B: EOF-catch で break(go の現行)**: 変更最小だが、要素途中の EOF と境界 EOF を
  区別できず不正入力を受理する。ipv6_invalid/std-io の fail として実測済み。却下。
- **C: RuntimeState に入力総長を持たせ end - offset >= n で判定**: 総長は subrange では
  既知でもトップレベル streaming では未知。streaming を捨てるか結局 lookahead に
  帰着する。却下。
- **D: peek/lookahead reader を要求(ADR 0037 の一般化。go は bufio.Peek、zig は
  生成 PeekReader アダプタ)**: 判定は正確になるが reader に能力要求が付き、
  backend ごとに wrap インフラが要る。offset 照合(本案)が同じ厳密さを
  無要求で達成できるため、until-eof 用途では過剰。remaining 式の用途にのみ残す。
