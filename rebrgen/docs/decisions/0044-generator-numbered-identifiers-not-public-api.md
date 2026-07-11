# 生成器採番 (nil-name) の宣言は生成コードの public API に含めない

## 日付

- 判断時期: 2026-07-11 (ebm2java 追加時。ebm2go には同種の慣行が先行して存在)
- 文書化: 2026-07-11

## 判断

EBM 上で名前が nil (= .bgn 著者が命名しておらず、識別子が tmp{id} 形式で
採番される) の宣言 — union 格納フィールド、匿名 variant-arm struct、
hoist された anon-inner accessor 等 — は、生成コードの public 面から
隠す。言語のアクセス制御があるなら強制する (Java: private)。
生成コードの public 面は「.bgn 著者が書いた名前 + property accessor +
encode/decode」だけにする。

## 動機

- **easy to write and read**: 生成コードの利用者 (IDE 補完、javadoc 等)
  に tmpNN が並ぶのは、内部表現の露出であり API の可読性を壊す。
  encode/decode 本体の機械的な冗長さは「オブジェクトコード」として
  許容する一方、型シグネチャに現れる名前は spec 著者の語彙に揃える。
- 内部表現 (union の格納方式等) を将来変更する自由度を保つ。

## 具体例

- ebm2java: `is_nil(field_decl.name)` / `is_nil(struct_decl.name)` で
  private/public を切替。hoisted anon-inner accessor
  (`hoisted_accessor_inner_ident` が返るもの) も private。
  Java の private nested member は同一トップレベルクラス内から
  アクセス可能なので、生成参照は全て外側クラス内に閉じていて成立する。
- ebm2go: nil-name の setter/getter は小文字始まり (unexported) で emit
  される (同じ原則の Go 版)。

## これは X を意味しない

- 「tmpNN 識別子を消す」ではない。生成コード内部では引き続き使う。
  隠すのは可視性であって存在ではない。
- 全 backend への即時義務ではない。アクセス制御の無い言語 (python 等) は
  命名規約 (`_` prefix 等) 相当の手段があれば従う、なければ現状維持。
- property accessor は nil-name 由来でも public のままにする場合がある
  (encode/decode のように公開契約の一部であるもの)。基準は「利用者が
  正常系で呼ぶ必要があるか」。
