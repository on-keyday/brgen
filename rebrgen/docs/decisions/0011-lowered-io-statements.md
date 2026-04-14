# LOWERED_IO_STATEMENTS による複数 IO 表現の提供

## 日付

- 判断時期: 2025年8月末〜9月上旬（8/25にlowered表現の追加開始、9月上旬に集中的に開発）
- 文書化: 2026-04-15

## 判断

StatementKind.LOWERED_IO_STATEMENTS を導入し、MULTI_REPRESENTATION として
複数の lowered IO 表現を生成して言語実装が最適なものを選べるようにした。

## 動機

- **write once generate any language code**: 言語ごとに最適な IO 戦略が異なる。
  単一の IO 表現を押し付けるのではなく、複数の表現を提供して
  各言語実装が自分に合ったものを選択できるようにした。

## 具体例

- 通常のケースでは READ_DATA/WRITE_DATA の chain がそのまま使われる。
- LOWERED_IO_STATEMENTS が活躍するのは、複数の lowered 表現が生成されるケースで、
  各言語実装が最適な表現を選択する。

## これは X を意味しない

- すべての IO が LOWERED_IO_STATEMENTS を経由するわけではない。
  通常は READ_DATA/WRITE_DATA の chain で十分。
