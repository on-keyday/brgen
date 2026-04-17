# 多言語対応箇所で言語リストをハードコードしない

## 日付

- 判断時期: 2026-04-17
- 文書化: 2026-04-17

## 判断

CI・スクリプト・設定ファイルで「対応言語（ebm2&lt;lang&gt;）を列挙する」必要がある箇所は、
必ず単一の SSOT (`rebrgen/test/unictest.json` や `build_config.json` など)
から動的に導出する。YAML や他ツールの内部に言語名を直書きしない。

## 動機

- **write once generate any language code**: rebrgen の最終目標は多言語コード生成であり、
  対応言語は**これからも増え続ける**前提で設計する。言語追加の摩擦を CI や周辺ツールに持ち込まない。
- **追加の影響半径を一箇所に閉じる**: 新言語を追加するたびに YAML/スクリプト複数箇所を書き換える
  運用は、追加忘れ・整合ずれ・レビュー負荷の温床になる。
- **レビューしやすさ**: PR で言語リストに差分が出るのは SSOT の 1 行だけ、という状態を維持する。

## 具体例

### CI の unictest matrix (実装済み)

`.github/workflows/unictest.yaml` は matrix 値を直接書かず、
`rebrgen/script/list_unictest_runners.py` が `test/unictest.json` を読んで runner 名配列を
`$GITHUB_OUTPUT` に出力し、`fromJSON` 経由で matrix に展開する。

```yaml
jobs:
  list-runners:
    outputs:
      runners: ${{ steps.r.outputs.runners }}
    steps:
      - run: echo "runners=$(python script/list_unictest_runners.py)" >> "$GITHUB_OUTPUT"
  unictest-shard:
    strategy:
      matrix:
        runner: ${{ fromJSON(needs.list-runners.outputs.runners) }}
```

新言語を足すときは `test/unictest.json` に 1 エントリ追加するだけで、CI は自動追従する。

### 今後寄せていきたい箇所

- **CI の toolchain setup**: 現状 `unictest.yaml` の shard job は Ninja/Clang/libc++/Zig を
  毎回全部インストールしている。本来は「そのランナーが必要とする toolchain だけ」入れたい。
  各ランナー側（`src/ebmcg/ebm2<lang>/` など）に toolchain 要件メタデータを持たせ、
  matrix 展開時にそれを読んで setup step を分岐する、という方向に寄せる余地がある。
- **build_config.json の `CODEGEN_TARGET_LANGUAGE` / `INTERPRET_TARGET_LANGUAGE`**:
  現在は手書き配列。ソースツリー (`src/ebmcg/`, `src/ebmip/`) から自動導出できるなら
  そちらに寄せる。少なくとも「YAML で言語列挙」と同じ筋で扱う。
- **release artifact の列挙**: 将来リリース成果物を言語ごとに分けるようになった場合も、
  同じ原則で SSOT から導出する。

## これは X を意味しない

- **ADR のスコープは「対応言語の一覧」のハードコード禁止に限る**。
  toolchain 要件をどう表現するか（言語側メタデータか、CI YAML 直書きか）は別軸の議論であり、
  この ADR は立場を固定しない。理想像（上記「今後寄せていきたい箇所」）と現状との差は
  時間をかけて縮めるもので、本 ADR が即座の refactor を命じるものではない。
- **既存の全箇所を即座に refactor する決定ではない**。
  新規・修正時に「言語リストをハードコードせずに済むか」を検討する運用規範であり、
  動いているコードを急いで書き直すものではない。
- **SSOT を必ず `test/unictest.json` にする決定ではない**。
  文脈ごとに適切な SSOT（build 設定、ディレクトリ構造、別の JSON など）を選ぶ。
  重要なのは「**どこか一つ**を SSOT として、他が派生する」構造であること。

## 代替案

### A. YAML の matrix に言語名を直書き

- 初期検討案。短期的には最速。
- 却下理由: 言語追加時の workflow 改修が毎回必要になり、
  「言語が増え続ける」前提と真逆のフリクションを産む。
  レビューでも言語追加 PR に CI 変更が混ざり続ける。

### B. 別ファイル（例: `.github/unictest_languages.yml`）に言語リストを置く

- YAML から言語リストだけ分離する折衷案。
- 却下理由: `test/unictest.json` と**二重管理**になる。追加漏れの温床。
  SSOT を増やす方向の解は採らない。
