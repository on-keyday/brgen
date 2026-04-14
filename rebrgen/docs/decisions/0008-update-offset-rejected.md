# UPDATE_OFFSET の StatementKind 追加を撤回した

## 日付

2026-04-15

## 判断

alignment 等のためのストリーム上の絶対オフセットカウント処理を
IR 層で共通化する（UPDATE_OFFSET を StatementKind に追加する）ことを試みたが、
撤回して言語固有実装内で行う方針にした。

## 動機

- **write once generate any language code**: オフセットカウントは各言語で共通の処理なので
  IR 側で共通化したかった。しかし実際にはうまくいかなかった。

## 具体例

- UPDATE_OFFSET を StatementKind に追加し、適切な位置に挿入しようとした。
- しかし LOWERED_IO_STATEMENTS との相性が悪く、多重カウントが発生した。

### 問題の本質

LOWERED_IO_STATEMENTS は各言語実装が最適な IO 戦略を選べるようにしている。
その結果、ストリームが実際に increment されるタイミングが言語実装ごとに異なりうる。
UPDATE_OFFSET をどこに挿入すれば正しいかが自明ではなく、
IR 層で一意に決められなかった。

## これは X を意味しない

- 「IR 層での共通化は常に失敗する」ではない。
  オフセットカウントは IO 戦略と密結合しているという特殊事情による。
  他の共通処理（is_mutable 判定など）は IR 層での共通化が適切。
- LOWERED_IO_STATEMENTS の設計が悪いわけではない。
  言語ごとに最適な IO 戦略を選べるという利点とのトレードオフ。

## 代替案（採用）

- 各言語の ebm2xxx 実装内でオフセットカウントを行う。
  IO 戦略と密結合した処理なので、言語固有実装のほうが正確に制御できる。
