# unictest 結果の推移を orphan ブランチに蓄積する

## 日付

2026-06-20

## 判断

unictest の CI 結果を、コミット単位で `unictest-history` という orphan ブランチに
追記蓄積する。1 run = `runs/<sha>.json`（全 runner の per-case 結果を trim したもの）
+ 派生集計 `summary.jsonl`。記録は `brgen-build` 完了後に走る独立ワークフロー
`unictest-history.yaml`（`workflow_run` トリガ）が行う。

## 動機

- **回帰の属性化**: 従来 Pages にデプロイされる `test_results.json` は毎回上書きされ、
  「前回どうだったか」が残らない。実際 ebm2rust の借用回帰（[[0034-mutation-analysis-borrow-vs-own]]）
  をデバッグした際、公開ベースラインが現 HEAD と同一で「いつ・どのコミットで flip したか」を
  追えず苦労した。コミット鍵の履歴があれば即特定できる。
- **flaky 検出**: コード無変更で pass↔fail を往復するケース（`status_interpretation ==
  "test internal error"` 等）を、履歴の flip として後から識別できる。
- `write once generate any language code`: 全 runner（ebm2c/cpp/go/rust/...）を 1 レコードに
  まとめるので、言語横断の劣化も 1 箇所で追える。

## 具体例

- 保存先: 同一 repo の orphan ブランチ `unictest-history`（main と歴史を共有しない、
  データ専用）。`runs/<sha>.json` は commit ごとに一意なので並行 run でも内容衝突しない。
  `summary.jsonl` は runs から毎回再生成する派生物なので、rebase-retry 下でも衝突しない。
- 記録経路: `brgen-build` の `unictest-results` アーティファクトを `workflow_run` 後に
  run-id 指定で取得 → `rebrgen/script/record_unictest_history.py` で trim & 集計 → push。
- snapshot は per-case で `runner/input/option/format/success/status_interpretation` を保持。
  **失敗 case のみ** triage 用に `output_stdout/stderr`（各 32KB cap）と先頭エラー行も残す。
  成功 case はログを落とす（"compilation successful" は無価値）。全 runner 込みの worst-case で
  非圧縮 ~1.1MB / git 圧縮 ~60KB/run、連続 run は delta 圧縮でさらに小さい。

## これは X を意味しない

- **回帰ゲートではない**（現時点では）。記録のみで CI を落とさない。flaky による偽陽性を
  避けるため、ゲート化（newly-FAIL で fail させる）は quarantine list や「2 連続 fail で初めて
  落とす」等の flaky 耐性を入れてからにする。差分の可視化（newly-FAIL / newly-PASS / 新規 failing /
  flaky 候補を Job Summary に出す `diff_unictest_history.py`）は実装済みだが非ゲート。
  ダッシュボード（input×commit ヒートマップの Layer 3）は別途。
- **完全な実行ログのアーカイブではない**。失敗 case の全文ログは 32KB cap で残すが、それを
  超える尾部や成功 case のログは保持しない。長大な全文は Actions のログ/アーティファクト
  保持期間に委ねる。

## 代替案（あれば）

- **Actions Artifacts に保存**: 保持期間が最大 90 日で長期履歴に不適。却下。
- **main に結果を commit**: main の履歴が CI 自動コミットで汚れ、push がさらに CI を
  トリガしてループする。orphan ブランチで隔離。却下。
- **別 repo に保存**: main repo は完全に綺麗になるが repo と権限管理が増える。solo 運用では
  同一 repo の orphan ブランチで十分。却下。
- **inline 記録（unictest-merge job 内で push）**: 同一 run でアーティファクト取得でき簡潔だが、
  reusable workflow の権限交差のため build.yml 側に `contents: write` 付与が必要で、重い
  テスト job に write トークンが及ぶ。独立 `workflow_run` ワークフローにして最小権限に隔離した。
- **github-action-benchmark / Allure**: 前者は名前付きメトリクスの時系列向けで per-case
  ヒートマップに合わない。後者は run 中心で履歴永続化に結局同じブランチ技が要り、ホスティングも
  重い。自作の commit 鍵スナップショットの方が用途に合う。
