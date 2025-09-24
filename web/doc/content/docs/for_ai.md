---
title: "For AI write"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# For AI write

生成系 AI に brgen フォーマットで書かせるためのテンプレート(TODO(on-keyday): お試し、要改善)

## 仕様ベース

```
上記の仕様を元に以下の形式でフォーマットを記述してください。

# これはコメントです
format A: # これはフォーマット宣言です。Aはフォーマット名を表します。より仕様にあった名前をつけることが推奨されます。
  .. # フィールドが何も無い場合はインデントして`..`を書いてください

format B:
  a :u8 # フィールドは「変数名 :型名」で定義します。これは符号なし8bit整数型のフィールドを定義しています。
  b :i16 # これは符号あり16ビット整数型のフィールドです。なお次に述べる例外のない限りはビッグエンディアンとして解釈します
  c :ul32 # これはリトルエンディアン符号なし32bit整数型のフィールドです。整数型は[us][bl]?[0-9]*

format C:
  fixed_array :[32]u8 # これは配列型のフィールドです。この配列はバイナリデータ表現上では長さ情報を含まず単に32バイトのバイト列のみに変換されます
  len :u8
  data :[len]u8 # これも配列型です。配列型の長さには単純な数値のみでなく他のフィールドや後述する変数、四則演算やビット演算などの式を使えます。

format D:
　　type :u8
    if type == 1: # これは条件分岐です。条件分岐を用いることでさまざまな
        len :u8
    else:
        len :u32

    data :[len]u32 # また条件分岐内で定義したフィールドはunionとなり、外側のスコープで使用することも可能です

    match type: # こちらも同じく条件分岐として使えます
        1 => additional_data :u8 # =>を使った形式では単一のフィールドのみを記せます
        2: # :を使った形式では複数のフィールドを記せます
            additional_data_len :u8
            additional_data :[additional_data_len]u8

format E:
    prefix :u8
    data [..]u8 # これは末尾終端パターンです。この場合、dataは入力終端までのデータを表します

format F:
    prefix :u16
    data :[..]u8 # このような場合、dataは末尾2バイトを除いた入力データすべてを表します
    authentication_code :u16

```
