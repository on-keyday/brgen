# 出口の仮置きと復元しやすさ

draft。2026-08-30 の議論の記録。lowering 側の出口をどう仮置きするかと、
EBM で起きた「EBM → bgn は戻せない」を新しい出口でどう避けるか。
完全可逆は目標にしない。**最低限、復元しやすくしておく**が要件。

## EBM が戻せない理由の分解

1. **破壊的書き換え** — 原木を運ばず、field → read/write 文、union →
   property という別語彙への再符号化で、元の書かれ方が残らない
2. **方向複製** — 1 つの原文式が encode / decode / getter / setter 用に
   複製され、同じ原文から来たことを覚えていない
3. **合成識別子** — EBM ID 埋め込みの生成名が原文の名前空間と切れる
   (改番ノイズの原因でもある)

## 出口 v0 = nast_wire

nast_wire (arena + side tables の無損失直列化、wiregen が nodes.json から
自動生成) を当面の出口と宣言する。根拠:

- 311 ファイルの往復 0 mismatch が毎日の回帰で回っている
- 消費者が既に 2 系統ある (C++ の from_json / TS の nast_nodes ローダ)
- lowering 相当の情報 (Requirements / UnionLayout / ConstantValue) は
  すべて side table = 原木への注釈として積んでおり、原木を壊す変換を
  していない。lowering を進めても当面は表の追加で乗り、出口の形が
  変わらない
- bgn の復元は spine の pretty-print で近似できる。完全な逆変換が
  無くても「どの解析結果がどの原文から来たか」は常に id で辿れる
  (source map の土台になる)

## .bgn への逆変換 (実装済み 2026-08-30)

`parse/unparse.{h,cpp}` が木から .bgn を書き戻し、`nast_unparse_test` が
parse → unparse → 再 parse → **structural 比較** → もう一度 unparse して
テキスト不動点、まで見る。`example/` 311 ファイル (構文エラー入りの 3 を除く
全部) で **311 ok / 0 mismatch / 0 unstable**。

- structural は compare.h に足した 3 つ目のモード。equivalent からさらに
  weak を丸ごと飛ばす (別々に parse した木では weak の id が一致しない)
- コメント・空行・桁は残らない。インデントは 4 空白に正規化。目標は
  原文の再現ではなく **再 parse で同じ形の木になること**
- parse が形を畳んで原文が一意に戻らないところは、再 parse で同じ木に
  なる側へ寄せて書く: Metadata の代入形 / `<u8>(x)` と `u8(x)` の
  is_explicit による書き分け / match 分岐の `=>` と 1 文ブロック /
  enum メンバの値は raw_expr があるときだけ

**逆変換が炙り出した情報落ちが 1 件**: SpecifyOrder が「どの指定か」
(input.endian / input.bit_order / ...) を捨てていた。ノードに name を
持たせて塞いだ。これは「落ちる情報が見つかったらノードに持たせて塞ぐ」の
実例そのもの。

書き出しの際に要った改行の調整が 2 つ: 括弧の中身がブロックを開く
(`(if ...: ... else: ...)`) ときの閉じ括弧と、範囲式の start がブロックを
開くときの演算子。どちらも parse が文末の改行を跨いで式を続けるため。

## 復元しやすさを保つ規則

lowering が本格化して合成ノードが増えても保つもの:

1. **lowering は追加が基本**。原木は落とさない。変換結果は表か、原木を
   参照する新ノード
2. **合成ノードは origin への weak 逆参照と loc を必ず持つ**。builtin
   module の file 0 慣行、union field の初出 loc は既にこの形。慣行から
   規則へ昇格させる
3. **方向複製は「同一 origin からの派生」を表で保持する**。encode 版と
   decode 版がそれぞれ無関係に生まれない

## 失うものと反論

EBM 無改修原則が守っていたのは backend の front スキーマからの絶縁。
nast_wire を出口にすると nodes.json の変更が出口形式に直結する。
反論: wire codec は wiregen の自動生成 (brgen 自身で自分の IR を定義する
self-hosting) なので、スキーマ進化の代金は手書き EBM + 多数 backend の
世界と桁が違う (Requirements の 12 フィールド化も再生成 1 回で wire まで
通った)。

本当に残るコスト:

- **出口サイズ** — 原木ごと運ぶ
- **読み方の案内** — spine 全体は生成器には過剰情報で、EBM は絞って
  見せる役を兼ねていた。backend が何をどの順で読めばよいかは、いずれ
  表の設計 (lowering 表の語彙) で答える必要がある

## 未決

- 出口 v0 の最初の実験的消費者を何にするか (旧 JSON AST への部分ブリッジ /
  骨組み printer / 玩具 backend)
- lowering 表の語彙 (backend への「読み方の案内」の実体)
- 方向複製の派生表の形 (段階 2 の encode 意味論と絡む)
