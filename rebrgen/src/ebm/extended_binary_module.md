# Extended Binary Module 基礎設計

1. 目的
   本文書は Extended Binary Module の設計思想及び基礎設計について述べるものである。詳細設計は extended_binary_module.bgn の定義そのものであるためここでは詳しくは言及しない。

2. 中央集権テーブル
   データ構造としては各オブジェクト(Identifier,StringLiteral,Statement,Expression,Type)の中央集権のデータテーブル+ID による参照によるグラフ構造である。
   これはバイナリフォーマットをできる限りコンパクトに保ちまた解析等が行いやすくなるように設計されている。
   ネストされたデータ構造は
   また Block,Expressions,Types などは基本は ID の配列でありデータそのものの配列ではない。

3. Lowered Statement
   高レベルの IR 表現をより基本的な構造で定義しなおしたものを Lowered Statement と呼ぶ。
   LLVM などのやつとは異なりあくまで高レベル表現を別のより基本的な高レベル表現に直したものである。(将来的にはさらに低レベルにするかもしれないが今のところはそうなっている)
   LoweredStatementRef 及び LoweredExpressionRef を境界として低レベル表現が格納される。
