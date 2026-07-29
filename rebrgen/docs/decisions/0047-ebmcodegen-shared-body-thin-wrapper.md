# ebmcodegen の per-lang 生成物は共有 body + thin wrapper で emit する

## 日付

- 判断時期: 2026-07-29
- 文書化: 2026-07-29

## 判断

ebmcodegen が各 `ebm2<lang>` に生成していた `main.cpp` (77k 行) / `codegen.hpp`
(6.5k 行) の言語別フルコピー (18 生成器 × 約 83.5k 行 ≒ 150 万行) を廃止し、

- **共有 body**: `src/ebmcodegen/generated/class_{codegen,interpret}_{source,header}.inc`
  に言語非依存の本体を 1 部だけ生成する。名前空間は `CODEGEN_NAMESPACE` マクロで
  パラメータ化する。
- **thin wrapper**: 各言語の `main.cpp` / `codegen.hpp` は
  `#define CODEGEN_NAMESPACE ebm2<lang>` + `#define CODEGEN_LANG_NAME "<lang>"` +
  共有 body の `#include` だけの数行に置き換える。

ツール側は `--mode <class-mode>-{wrapper,body}` の GenerateMode 追加で出し分ける
(成果物の種類はフラグでなく mode で表す既存規約に従う)。

## 動機

- 従来方式は「言語名トークン置換以外テキスト同一」のファイルを 18 部コミット
  していた (正規化 diff で main.cpp は差分ゼロ、codegen.hpp は
  `lang_name = "<lang>"` / `file_extensions` の 2 行のみ)。EBM 構造変更のたびに
  150 万行が再生成され、git churn・grep ノイズ・レビュー対象の膨張を招いていた。
- フック側が消費するインターフェース (`DEFINE_VISITOR`, `CODEGEN_VISITOR(name)`,
  `CODEGEN_NAMESPACE`) は既に全てマクロ間接化されており、パラメータ化の障害が
  なかった。生成テキスト側だけが言語名を ~9,700 箇所ハードコードしていた。
- 本変更で追跡対象の生成物は共有 body ~167k 行 + wrapper 18×7 行になる
  (約 94% 削減)。

## 具体例

- wrapper (`src/ebmcg/ebm2go/main.cpp`):

  ```cpp
  #define CODEGEN_NAMESPACE ebm2go
  #define CODEGEN_LANG_NAME "go"
  #include <ebmcodegen/generated/class_codegen_source.inc>
  ```

- 共有 body 内の per-hook ブロックは `CODEGEN_NAMESPACE::Visitor<...>` を使い、
  文字列リテラルに言語名が要る箇所 (BaseVisitor::program_name,
  TEMPORARY_CHECK_MACRO の期待文字列, webworker_name) は
  `EBMCG_STRINGIFY(CODEGEN_NAMESPACE)` (stub/util.hpp で定義) で構成する。
  期待文字列の分割結合が展開後 stringify と一致することはコンパイル実験で確認済み。
- フック発見の `__has_include("visitor/<hook>_class.hpp")` は「ファイル相対」から
  「per-target include path」に移行: 生成 CMakeLists が
  `target_include_directories(ebm2<lang> PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})`
  を持ち、同一 body が言語ごとに異なるフック集合を拾う。
- `ebmtemplate.py` の「新フック追加時に main.cpp を touch」ワークフローは
  wrapper が main.cpp なので無変更で機能し、再ビルド粒度も per-lang のまま。
- `script/ebmcodegen.py` は全出力を write-if-changed にし、body 不変時の
  無駄な全言語再ビルドを避ける。

## 制約 (quote-include の探索順序)

quote-include は「その directive を含むファイルのディレクトリ」を最優先で探索
するため、**共有 body の置き場所に `visitor/` サブディレクトリを置いてはならない**。
置くと全言語がそのフックを言語自身の `visitor/` より優先して拾う (clang 22 で
実証済み)。`src/ebmcodegen/generated/` はこの不変条件を守る専用ディレクトリで
あり、`default_codegen_visitor/` 直下 (visitor/ を持つ) には置けない。

## これは X を意味しない

- **コンパイル時間の削減ではない**。18 TU がそれぞれ共有 body 全体を前処理・
  インスタンス化する構図は不変 (PCH は従来から codegen_pch で共有)。削るのは
  リポジトリ上の重複テキストと regen churn である。
- **名前空間の統一ではない**。`ebm2go::` 等の per-lang 名前空間は維持される
  (wrapper が解決)。単一名前空間化は clangd のインデックスで 18 言語の同名
  特殊化が混線するため却下した。
- 非クラスベースのレガシー `codegen` モード、`ebmgen-visitor` モード、
  `default_codegen_visitor/codegen.hpp` (ebm2all IDE 用 dummy header) は
  従来どおり非パラメータ化 emission のまま。

## 代替案

- **単一固定名前空間 + 完全同一ファイル**: wrapper すら不要になるが、追加削減は
  僅かで clangd の cross-lang jump 混線という IDE 体験悪化が upside に見合わない。
- **存在するフックのみ emit**: 唯一コンパイル時間に効く案だが、フック追加ごとに
  main.cpp 再生成が必要になり `__has_include` による動的フック発見 (touch のみで
  反映) のワークフローを壊すため見送り。
- **フラグ (--wrapper/--shared-body) での出し分け**: mode と直交するフラグは
  無効組合せの検証を増やすだけで、「成果物の種類 = mode」の既存規約に反するため
  GenerateMode 追加に変更した。
