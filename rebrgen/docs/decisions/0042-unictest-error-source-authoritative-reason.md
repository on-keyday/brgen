# unictest の失敗理由は発生源が権威的に emit する（UNICTEST_ERROR 規約）

## 日付

2026-07-09

## 判断

各 `unictest.py`（および共有ハーネス `unictest_setup.py`）は、失敗パスで 1 行 `UNICTEST_ERROR: <phase>: <reason>` を print する。CI job summary レンダラ（`script/render_unictest_summary.py`）はこの行を grep して per-case の失敗理由を表示する。理由の抽出を下流（レンダラ）の正規表現推測に頼らず、フェーズを知っている発生源が権威的に出す。

## 動機

- **3 目標には直接紐づかない**。これは format 表現力でも書きやすさでも多言語生成でもなく、**開発ループの可観測性**（unictest の失敗が CI run ページで一目で分かる）の改善。MVP→実プロダクト移行における「correctness を unictest で通すことを重視する」という運用価値に紐づく。
- きっかけは「CI summary に HTML を貼る仕組み（GitHub Job Summary）を unictest 結果の一時表示に使えないか」という発想。表示先が決まると、次に「各失敗の理由をどう1行に落とすか」が問題になった。
- レンダラ側で stdout blob を正規表現で漁る初期実装は、blob が常に `UNICTEST_*` env dump + ハーネス scaffolding で始まるため「先頭行が常にノイズ」になり、言語ごとにパターン追随が必要で脆かった。発生源は (a) どのフェーズで落ちたか (b) どの subprocess の出力が理由か を正確に知っているので、そこで出すのが SSOT。

## 具体例

- 共有ヘルパ `script/unictest_report.py`: `fail(phase, text=None, code=1)`（emit して exit）/ `report(phase, text=None)`（emit のみ）。`pick_line` が **scoped な** subprocess 出力から主要エラー行を選ぶ（1 行入力はそのまま）。
- ebm2go 実行失敗: `main.go` が `Decode error: X` を stderr に出し exit 10 → `phase = {10:'decode',20:'encode'}.get(rc,'run')`、理由は `proc.stderr`。
- ebm2cpp コンパイル失敗: cmake/ninja の混在出力を capture し、`pick_line` が `generated.hpp:571:35: error: no matching conversion...` を ninja 進捗行から選別。
- 出力 mismatch / no-output は `unictest_setup.py` が集中して emit（`compare: output mismatch (in NB vs out MB, first diff @ 0xK)`）。各 unictest.py はこの2フェーズをやらない。
- import 解決: `unictest_setup.py` が子 spawn 時に `script/` を PYTHONPATH 注入するので、各 unictest.py は素の `import unictest_report`。sys.path 操作は書かない。
- phase 語彙: `codegen` / `compile` / `decode` / `encode` / `run` / `compare` / `harness`(未実装スタブ) / `setup`。exit 10/20 は decode/encode にマップするが、runner により 20 が存在しないこともある（ebm2rmw は `interpret.hpp` を確認し 10 のみ）。

## これは X を意味しない

- **理由が無い失敗を推測で埋めるわけではない**。レンダラは `UNICTEST_ERROR:` 行を grep するだけで、blob への heuristic fallback は持たない。`UNICTEST_ERROR:` が無い失敗（unictest.py 自体の想定外例外・kill/timeout など、構造化されずに死んだケース）は理由欄を `—` にする。**誤った推測を出すより正直な `—` の方が良い**（regex fallback は当初残したが、blob 推測を復活させ誤誘導しうるため撤去した）。`—` でも status 列が分類し full log は artifact にあるので実害は無く、むしろ「予期せぬ失敗」を正直に示す。
- **ハーネス（Rust の `unictest`）が何かをパースするわけではない**。`unictest.rs` は従来通り exit code で pass/fail を判定し stdout/stderr を丸ごと記録するだけ。`UNICTEST_ERROR:` はその記録の中の 1 行に過ぎず、レンダラだけが解釈する。ハーネスの責務は増えていない。
- **各 runner が独自のエラー整形を実装するわけではない**。理由テキストは基本、対象 subprocess の captured stderr を verbatim（複数行なら `pick_line` が 1 行選ぶ）。runner がやるのは「どの subprocess の出力か」と「phase ラベル」を渡すことだけ。
- **exit code のセマンティクスは変えていない**。移行は emit を追加しただけで、各失敗箇所の exit code は完全に維持（`fail(..., code=元のcode)`）。pass/fail 判定・failure_case の扱いは不変。
- **`harness` phase は「バグ」を意味しない**。ebm2llvm/p4/z3/json/ascii は test ロジックが未実装のスタブで、これは既知の未完成状態を正直にラベルしているだけ。

## 代替案

- **レンダラ側 regex のみ（発生源を触らない）**: ハーネス無改修で済むが、混在 blob への推測で言語ごとに脆く追随が必要。却下（これが初期実装で、破綻した）。
- **中央集約（`unictest_setup.py` / `unictest.rs` が子出力をパースして理由を抽出）**: 1 箇所で済むが、ハーネスは言語非依存でフォーマットを知らず、結局 blob への推測を中央でやるだけ。フェーズ情報も失う。意味知識のある場所＝各 unictest.py が正しい置き場所。
- **exit code だけで phase を表し、具体行は出さない**: 完全に regex-free だが「decode error (exit 10)」程度の粗い情報しか出ず、`cannot convert X to int` のような具体エラーが失われる。coverage（具体性）を優先して却下。
