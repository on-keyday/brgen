# default_codegen_visitor の役割と言語固有 visitor の線引き

## 日付

2026-04-15

## 判断

default_codegen_visitor は言語非依存な構造的処理とデフォルトの hook 差込口を提供し、
各言語固有の実装は entry_before_class.hpp でのオーバーライドや
visitor/ 内の個別 hook ファイルで行う。

## 動機

- **write once generate any language code**: 言語に依存しない共通処理を一箇所にまとめる。
- 各言語の実装量を減らし、新言語の立ち上げを速くする。

## 具体例

- default_codegen_visitor: 構造的なトラバース、言語非依存な展開処理、
  std::function ベースの hook 差込口（*_custom, *_visitor, *_wrapper）を設置。
- entry_before_class.hpp: 短い処理や設定系はここで hook をオーバーライドする。
- visitor/Statement_FOO_class.hpp: 長くなりがち、かつ言語固有な処理を個別ファイルに分離。

## これは X を意味しない

- default と言語固有の線引きは絶対的ではない。言語数が増えるにつれて流動する。
  目安として 1-2 言語にしかない処理は各言語固有で、3 言語以上に共通するなら
  default への昇格を検討する。
