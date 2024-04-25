---
title: "Exec Model"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# 実行モデル

現時点での brgen の実行モデルを定義する。
このモデルは将来変更される可能性がある。
この実行モデルではバイナリデータの読み込み、書き込みの状態と言語の各構文との対応を示す。
なお実行モデルはあくまで抽象機械に対する操作を表しており、
それをどう実装するか(具体的な入出力インターフェイスやデータの保持方法等)は実装者次第である。

用語を定義する

- input - 入力のバイト列のこと
- output - 出力のバイト列のこと
- IO - input/output の両方のこと、その略
- stream - IO のうち終端が決定できないもの
- block - IO のうち終端が決定できるもの
- bit_stream - IO をビット単位で扱うもの。なお bit_stream と言っているが stream の場合も block の場合もありえる。
- offset - IO の現在の読み取り、書き取り位置(バイト単位)
- bit_offset - bit_stream の現在の読み取り位置、書き取り位置(ビット単位)。offset と組み合わせて使う
- 実行 - IO や offset,bit_offset などになんらかの操作を加えたり、データを取り出したりすること
- instance - input の場合、実行の結果生じたデータを保持するもの。output の場合、実行の元となるデータを保持するもの

## input/output

1 byte=8 bit を前提として実行モデルを定義する。
以下例示があるが、input や output 内の数値に意味はなく説明用である。
例えば以下のフォーマットの場合。

```
format A:
    a :u8
```

実行モデルは次のように変化する

input 実行前

```
input = [ 0x10, 0x20 ]
offset = 0
instance = {}
```

input 実行後

```
input = [ 0x10, 0x20 ]
offset = 1
instance = { a: 0x10 }
```

ここで offset が 0 から 1 に変化するが、input にはなんの変化ももたらさないことに留意したい。
他の方法としては以降確実に offset が後退することがないことが保証できるならば実行後を以下のようにもできる。

```
input = [ 0x20 ]
offset = 0
instance = { a: 0x10 }
```

この例では input からデータを取り出し input の先頭を削除することで進行を表現している。

output の場合はどうかというと

output 実行前

```
output = []
offset = 0
instance = { a: 0x10 }
```

output 実行後

```
output = [ 0x10 ]
offset = 1
instance = { a: 0x10 }
```

となる。

## endian

続いて次のようなフォーマットを考える。

```
format B:
    b :u16
```

brgen では元々の開発動機がネットワークプロトコルフォーマット向けのものであるため、
既定ではビッグエンディアンとすることにしている。なお、明示的にエンディアンを指定していない場合はビッグエンディアンがデフォルトだが、
ジェネレーターはその動作をコマンドラインオプション等で変更できるようにしても良い。
この既定の動作の実行モデルは以下である。

input 実行前

```
input = [ 0x12 , 0x34 ]
offset = 0
instance = {}
```

input 実行後

```
input = [ 0x12 , 0x34 ]
offset = 2
instance = { b: 0x1234 }
```

output 実行前

```
output = []
offset = 0
instance = { b: 0x5678 }
```

input 実行後

```
input = [ 0x56 , 0x78 ]
offset = 2
instance = { b: 0x5678 }
```

なお例えば以下のようにするとリトルエンディアンとして明示的に指定できる。

```
format B:
    b :ul16
```

この場合の実行モデルは以下である。

input 実行前

```
input = [ 0x12 , 0x34 ]
offset = 0
instance = {}
```

input 実行後

```
input = [ 0x12 , 0x34 ]
offset = 2
instance = { b: 0x3412 }
```

output 実行前

```
output = []
offset = 0
instance = { b: 0x5678 }
```

output 実行後

```
input = [ 0x78 , 0x56 ]
offset = 2
instance = { b: 0x5678 }
```

なお、この明示的にエンディアンを指定する構文で書かれたものはコマンドラインオプション等で変更するべきではない。

## 複数のフィールド

以上のモデルを複数フィールドの場合に適用する。

```
format C:
    c1 :u8
    c2 :u32
    c3 :u16
    c4 :u16
```

input

```
input = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 ]
offset = 0
instance = {}
```

```
input = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 ]
offset = 9
instance = { c1: 0x01, c2: 0x02030405, c3: 0x0607, c4: 0x0809 }
```

output

```
output = []
offset = 0
instance = { c1: 0x01, c2: 0x03040506, c3: 0x0708, c4: 0x090A }
```

```
output = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09]
offset = 9
instance = { c1: 0x01, c2: 0x02030405, c3: 0x0607, c4: 0x0809 }
```

なおリトルエンディアンの場合は以下。

input

```
input = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 ]
offset = 0
instance = {}
```

```
input = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 ]
offset = 9
instance = { c1: 0x01, c2: 0x05040302, c3: 0x0706, c4: 0x0908 }
```

output

```
output = []
offset = 0
instance = { c1: 0x01, c2: 0x02030405, c3: 0x0607, c4: 0x0809 }
```

```
output = [ 0x01, 0x05, 0x04, 0x03, 0x02, 0x07, 0x06, 0x09, 0x08 ]
offset = 9
instance = { c1: 0x01, c2: 0x02030405, c3: 0x0607, c4: 0x0809 }
```

## 配列

brgen は 2 種類の配列を定義している。静的配列と動的配列である。
まず静的配列の例を見せる。

```
format D:
    d :[4]u16
```

このように定数を指定すると静的配列になる。なお、定数は定義のパース時に値を決定できる場合であり、例えば以下の場合も静的配列として使うことが可能である。

```
N ::= 1 + 2 + 3 - 2
format D:
    d :[N]u16
```

このとき、N は定数かつその定義は `1+2+3-4` でパース時に決定可能であるため、N は 4 に置き換えられ、要素数 4 の静的配列として扱われる。
なお、定数として扱える範囲は brgen のパース時の計算能力の向上(対応している演算等の増加)によって範囲が広がる予定である。

なおこのときの実行モデルは以下のようになる。

input

```
input = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 ]
offset = 0
instance = {}
```

```
input = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 ]
offset = 8
instance = { d: [ 0x0102, 0x0304, 0x0506, 0x0708 ] }
```

output

```
output = []
offset = 0
instance = { d: [ 0x0102, 0x0304, 0x0506, 0x0708 ]  }
```

```
output = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 ]
offset = 8
instance = { d: [ 0x0102, 0x0304, 0x0506, 0x0708 ]  }
```

TODO(on-keyday): 続きを書く

# ビットフィールド

本言語ではビットフィールドが特別扱いされないという事になっている。
まあ筆者も他の類似ツールが`bitfield(16) { n :1, reserved :15 }`みたいな書き方しているのや capnproto とかいうフォーマットのやつがこの言語の書き方では
うまく表せないなあと気づいたときに返るべきかと思ったりはしているが、
現状こんな

```
format E:
    a :u2
    b :u2
    d :u4
```

なにも指定しないデフォルトの状態ではこれらは上位ビットから割り付けられ行く
マスク値で言うと a が 0xC0,b が 0x30,c が 0x0f である。

なおこの割り付け順序は以下のようにすると lsb からになる。

```
format F:
    input.bit_order = config.bit_order.lsb
    a :u2
    b :u2
    d :u4
```

この場合は
