# ebmcg/ と ebmip/ の分離

## 日付

- 判断時期: 2025年11月頃（ebm2rmw の追加でインタプリタ型とコンパイル型の違いが顕在化）
- 文書化: 2026-04-15

## 判断

コードジェネレーター（ebm2xxx）を ebmcg/（compiled）と ebmip/（interpreted）に分離する。

## 動機

- ebmcodegen が生成するスケルトンコードの違いに起因する。
  ebmcg/ では Result に CodeWriter をデフォルトで含める（コード文字列を組み立てて出力する）。
  ebmip/ では CodeWriter を含めない（インタープリタのように直接実行する用途）。

## 具体例

- ebmcg/: ebm2c, ebm2python, ebm2rust, ebm2go 等。EBM を読んでターゲット言語のソースコードを生成する。
- ebmip/: ebm2rmw 等。RMW はインタープリタなので CodeWriter が不要。
  代わりに visitor/Result.hpp に str_repr フィールドを追加してデバッグ出力に対応している。

## これは X を意味しない

- アーキテクチャ上の深い設計思想による分離ではない。
  ebmcodegen のコード生成テンプレートの都合による実際的な区分。
