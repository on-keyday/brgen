# ebmcg/ と ebmip/ の分離

## 日付

- 判断時期: 2025年11月頃（ebm2rmw の追加でインタプリタ型とコンパイル型の違いが顕在化）
- 文書化: 2026-04-15
- 意味論の明文化: 2026-04-17（ebm2ascii 配置議論を契機に broaden）

## 判断

コードジェネレーター（ebm2xxx）を ebmcg/（default codegen visitor を継承する側）と ebmip/（継承しない側）に分離する。

両者の差は **ebmcodegen が生成するスケルトンコードの差**（特に Result 型に CodeWriter を含めるか）に由来する実際的な区分であり、アーキテクチャ上の深い設計思想ではない。

## 動機

- ebmcodegen が生成するスケルトンコードの違いに起因する。
  - ebmcg/ では Result に CodeWriter をデフォルトで含める（コード文字列を組み立てて出力する典型的なコード生成用途）。
  - ebmip/ では CodeWriter を含めない（インタープリタ実装や、CodeWriter 中心の default が不適な出力のための軽量な骨格）。
- default codegen visitor の機構が重量級なため、それが不要・不適なケースを受け皿として分離しておくことで、
  通常の "コード生成" から逸脱する generator を追加しやすくする。

## 具体例

- ebmcg/: ebm2c, ebm2python, ebm2rust, ebm2go 等。EBM を読んでターゲット言語のソースコードを生成する。
- ebmip/:
  - ebm2rmw: RMW はインタープリタなので CodeWriter が不要。
    代わりに visitor/Result.hpp に str_repr フィールドを追加してデバッグ出力に対応している。
  - ebm2json: EBM をシリアライズしたデータ出力であり、ターゲット言語のソースコードを生成しない。
    CodeWriter 中心の default は不適で、ebmip 側が自然。
  - ebm2ascii（予定、ADR 0024）: RFC 風 ASCII ヘッダ図を出力する可視化 generator。
    制御構造・Expression 系の hook 群が丸ごと不要であり、default codegen visitor がオーバーキル。

## これは X を意味しない

- アーキテクチャ上の深い設計思想による分離ではない。
  ebmcodegen のコード生成テンプレートの都合による実際的な区分。
- **ebmip は interpreter 専用ではない**。略称の由来は ebm2rmw（interpreter）だが、
  実体は **「default codegen Result (CodeWriter 中心) が不適な generator 全般の受け皿」** である。
  現に ebm2json（シリアライズ出力）が既に同居しており、可視化・ドキュメント出力等も自然にここへ入る。
- 名前 "ip" の誤解可能性は認識しているが、2026-04 時点では改名は保留。
  住人が更に増え、"interpreter" の読みが実害レベルで誤解を招くようになった時点で改名を再検討する（ADR 0024 参照）。
