# EBM の FunctionAttribute に is_mutable フラグを持たせる

## 日付

2026-04-15

## 判断

関数（メソッド）が self の要素を変更するかどうかを ebmgen 側で事前計算し、
EBM の FunctionAttribute.is_mutable フラグとして埋め込む。

## 動機

- **write once generate any language code**: C++ の const、Rust の &self vs &mut self など、
  各言語で mutability の表現方法は異なるが、判定ロジックは共通。
  各 ebm2* が個別に判定するのは無駄で、EBM 側に情報を持たせれば一箇所で済む。

## 具体例

- C++: const メソッド修飾
- Rust: `&self` vs `&mut self`

判定対象の Statement: ASSIGNMENT, YIELD, APPEND, READ_DATA, ARRAY_TO_INT, INT_TO_ARRAY の
target が self メンバーを指しているかを走査。IF/LOOP/MATCH は再帰。
PROPERTY_SETTER, COMPOSITE_SETTER, VECTOR_SETTER は自明に mutable。

## これは X を意味しない

- 「全ての情報を EBM に持たせるべき」ではない。言語固有の判断は ebm2* 側の責務。

## 代替案

- ebm2* 側で個別に判定: 各言語で同じ走査ロジックを書くことになり、
  目標 3（write once generate any language code）に反する。
