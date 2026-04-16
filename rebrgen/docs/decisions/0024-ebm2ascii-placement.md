# ebm2ascii の配置と ebmip 改名の保留

## 日付

- 判断時期: 2026-04-17
- 文書化: 2026-04-17

## 判断

RFC 風 ASCII ヘッダ図を出力する ebm2ascii を **ebmip/ 側**に配置する。
ebmip の名前 "ip" の誤読可能性は認識しているが、**改名は今回は見送る**。
また、ebmcg/ebmip を解体してフラット化する案も見送る。

## 動機

- **easy to write and read**: ASCII 図生成は制御構造・Expression 系の hook 群が丸ごと不要。
  default codegen visitor（CodeWriter 中心）は完全にオーバーキルであり、
  ebmcg/ 側に置くと "default が合わない generator" として異物になる。
- ebm2json という前例が ebmip に既に存在する（ADR 0005 参照）。
  "シリアライズ・可視化・ドキュメント出力系は ebmip" のパターンが実質的に確立しており、
  ebm2ascii はこの系譜に自然に続く。
- 技術的な面白さ: IPv8 のようなジョークプロトコルを含む `.bgn` 定義から
  IETF 風 ASCII 図を機械生成できると、フォーマット仕様の破綻が
  **型レベル・レイアウトレベルで可視化**される。設計破綻検出ツールとしての副作用的価値がある。

## 具体例

- ebm2ascii は `src/ebmip/ebm2ascii/` に配置する。
- 実装対象の EBM ノードは MVP として以下に限定する:
  - `Statement_STRUCT_DECL`（format 単位のダイアグラム生成）
  - `Statement_ENUM_DECL`（side-table 併記）
  - `Type_INT`/`UINT`/`FLOAT`/`BOOL`/`ARRAY`/`STRUCT_REF`（セル描画）
- 出力は 2 形式に対応: IETF 風 ASCII 図（`--format=ascii`）と Markdown テーブル（`--format=table`）。
- ネスト型は `--inline-depth=N` で展開 vs 参照を切り替え。
- `Statement_IF`/`LOOP`/`MATCH`、Expression 系は MVP 範囲外。

## 詳細仕様（2026-04-17 追記）

### 2-pass アーキテクチャ

**抽出 pass（EBM → 中間表現）**
- 対象 format を走査して各フィールドを `[(name, bit_offset, bit_width, type_info, constraint?)]` のフラットリストへ落とし込む。
- ネスト型は `--inline-depth=N` に従って再帰展開するか、参照（`Ref<TypeName>`）として残すかを分岐。
- 制約（`version == 8` 等の immediate assert）は該当フィールドの `constraint` フィールドに添付、または format 全体に紐づけて保持。

**描画 pass（中間表現 → 出力文字列）**
- フラットリストに対する純粋な変換関数。状態は**関数引数で渡す**形式で、グローバル state は持たない。
- `--format=ascii` と `--format=table` の両出力は**同じフラットリストから**分岐するだけ。描画ロジックは二重化しない。

### CodeWriter の扱い

CodeWriter は futils 由来の汎用インデント付きライター。**ビットカーソル等の状態は絶対に CodeWriter に載せない**。
ebm2ascii 固有の状態・中間表現は `src/ebmip/ebm2ascii/visitor/` 配下の独立した型で管理する。

### ASCII 図の細部仕様

- **行幅**: 32bit 固定で開始。`--width N` は後付け。
- **可変長フィールド**: IETF 慣習に従い `~` で伸縮表現。長さ式は EBM の Expression をソース表記のまま文字列化してセル内に注釈（例: `Options (ihl*4-28 bytes)`）。
- **非バイト整列・行跨ぎ**: IETF 慣習どおり境界を跨いで描画。format 全体の bit 数合計が 32 の倍数にならない場合は**警告**を出す（エラーではない）。
- **制約の表現**: 図の直下に `Constraints:` セクションとして列挙。インライン注記は採らない。

### ネスト型の扱い

- **インライン展開（`--inline-depth >= 1`）**: 子型のフィールドを親ダイアグラムに再帰的に埋め込む。親フィールド名は脇にグループ注釈として表記。
- **参照（`--inline-depth=0`）**: 親には `fieldName: TypeName` とだけ書き、被参照型のダイアグラムを同じ出力に別ブロックとして並べる。

### 出力単位

- 1 format = 1 ダイアグラム。ファイル全体では**宣言順**に列挙する。
- トポロジカルソートは採らない（読み手の期待に沿う順序を優先）。

### 実装時に詰める残論点

- **Expression 文字列化の括弧・演算子優先順位**: 長さ式注釈の綺麗さのため、EBM の Expression AST から最小括弧で復元する処理が必要。具体仕様は実装着手時に決める。

## これは X を意味しない

- **ebmip が "可視化ツール置き場" になったわけではない**。
  ebmip の本質は依然として「default codegen visitor (CodeWriter 中心) が不適な generator 全般」であり、
  ebm2ascii は ebm2json と並んで「たまたま該当する住人」である。
- **ebmip の名前を永続的に維持する決定ではない**。
  "interpreter" の誤読が実害を出し始めた時点で改名を再検討する余地を残している。
  候補は `ebmlt`（lightweight）、`ebmmin`（minimal）、`ebmbare` 等。
- **ebm2ascii を full code generator として扱う決定ではない**。
  MVP は静的レイアウトに限定し、制御構造・動作意味の可視化は範囲外。

## 代替案

### A. ebmcg/ に統合

- "cg" を「generator 全般」と緩く読み替え、ebm2ascii を ebmcg/ に置く。
- 却下理由: default codegen visitor の重量級機構を継承してしまい、大量の hook が no-op になる。
  "default が合わない generator" をあえて ebmcg 側に置く動機が無い。

### B. フラット化（ebmcg/ebmip 解体）

- parent brgen の `src/tool/json2*` と同様、`src/ebm2<target>/` 直下に並べる。
- 却下理由: ebmcg/ebmip の境界は **ebmcodegen のスケルトン生成テンプレート差**という
  実装上の意味を持っており（ADR 0005）、解体するとこの差が視覚的に見えなくなる。
  新規 generator 追加時に「どちら側のスケルトンを使うか」の判断材料が失われる。

### C. 新カテゴリ ebmviz/ 新設

- 可視化生成器専用ディレクトリを作り ebm2ascii/ebm2mermaid/ebm2graphviz を集約する案。
- 却下理由: ebmip に既に ebm2json という "非 interpreter" 住人が居る時点で、
  新カテゴリは **3 つ目のスカスカディレクトリ**を作るだけ。
  ebmip の意味を broaden する方が筋が良い（ADR 0005 の明文化で対応済み）。

### D. ebmgen に `-f ascii` オプションとして組込み

- `-d debug.txt` と兄弟のように ebmgen 内部モードとして実装する。
- 却下理由: ebmgen は AST → EBM 変換ツールであり、EBM → 出力の責務を持つべきではない。
  ebm2json が独立ツールとして ebmip に存在する以上、ebm2ascii も同様の独立ツールが整合的。
