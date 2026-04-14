# ebmcodegen を直 #include パターンから class-based パターンに移行した

## 日付

- 判断時期: 2025年11月20日頃（class_based.cpp の初期実装、以降段階的に移行）
- 文書化: 2026-04-15

## 判断

ebmcodegen のコード生成方式を、直 #include パターンから class-based パターン（class_based.cpp）に移行した。
旧式パターンは温存しつつ、新規開発は DEFINE_VISITOR マクロによる class-based パターンで行う。

## 動機

- 当初は実装速度を優先し、直 #include パターンを採用した。
  これは完全にボイラープレートが 0 になる反面、IDE が死ぬ（補完・型情報が効かない）
  という問題があった。
- 言語ジェネレーターが増えるにつれて認知負荷が極大になっていった。
- **easy to write and read** に反する状態になったため、class_based.cpp を開発した。

## 具体例

### 旧式: 直 #include パターン

- visitor の .hpp ファイルが直接 #include される。スコープや型情報が暗黙的。
- ボイラープレート 0 だが IDE の補完が全く効かない。
- 今も default_codegen_visitor 等に一部残存。ebmcodegen の main.cpp にも旧式の生成エンジンが残っている。

### 現行: class-based パターン (class_based.cpp)

- DEFINE_VISITOR マクロ 1 つだけのボイラープレートで、旧式とほぼ変わらない記述量。
- ctx が明示的な型を持つため IDE の補完・型チェックが効く。
- 旧式パターンも「はめ殺し」的に温存し、既存コードの大規模書き換えを回避した。

## これは X を意味しない

- 旧式パターンが完全に廃止されたわけではない。
  default_codegen_visitor 等では旧式が残っており、段階的に移行している。
- DEFINE_VISITOR マクロの存在が「マクロ多用」の一環ではあるが、
  これは IDE 体験と記述量のバランスを取るための意図的な選択。

## 代替案

- 旧式パターンのまま続行: IDE 体験の悪化で開発速度が落ちていたため断念。
- マクロなしで完全に class ベース: ボイラープレートが増えすぎる。
  DEFINE_VISITOR 1 つで済むなら許容範囲。
