---
title: "Builtin"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# Builtin 関数

brgen(lang)のビルトイン関数について記述する

```
input.endian = endian
```

デフォルトのエンディアンを指定する構文である。
endian に指定されたエンディアンに変更される。
なお、endian の中身が定数ならば静的に決定され、
そうでないなら実行時に動的に決定するようなコードを生成する。

適用範囲はフォーマットごとである。

```
config.endian.big
config.endian.little
config.endian.native
```

これらは定数として解釈され、input.endian に対する指定として使う。
エンディアンを指定したい場合はこれらの定数を使用するべきである

```
a :Type(input = input.subrange(length,[offset]))
```

バイト単位の長さを指定する関数である。
length 引数でバイト長さを指定する。
offset 引数はオプションで、入力全体のオフセットを指定する。
例えばファイル入力であればファイルの先頭が 0 として扱われる。

```
a :Type(input.align = bit)
```

bit ビットアラインを強制する
バイトアラインを強制したい場合は`強制したいバイト数*8`でしていする

```
a :Type(input.peek= true)
```

フィールドの読み込みは行うが入力位置を進めないのを示すために使用する

```
input.offset
```

入力の現在位置を示す変数。
参照位置によって値が変わる

```
input.remain
```

入力の残りを示す変数。
参照位置によって値が変わる

```
input.get([Type],[length])
```

入力を読み取る関数。Custom フォーマットで使用する。
Type は型名(単一の識別子で表せる型のみ)分読み込むことを示す。省略すると u8 となる。
length は Type で示した型何個分かを示す。省略すると 1 となる。

length が静的に解決できて 1 の場合、Type 型の値を戻り値として返す。
length が動的か 2 以上の場合[]Type 型の値を戻り値として返す。

```
input.peek([Type],[length])
```

入力を読み取るが、位置を進めない。
他の点については`input.get`と同じである。

```
output.put(value)
```

値を出力する。Custom フォーマットで使用する。
value は任意の型の値をとることができる。

```
input.backward([length])
```

入力を length バイト読み戻す。
省略されると 1 バイト
