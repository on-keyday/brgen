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
top level に記述すると以降の全体に適用され、format 内部に記述するとその時点以降のエンディアンに影響する。

適用範囲はフォーマットごとである。

```
config.endian.big
config.endian.little
config.endian.native
```

これらは定数として解釈され、input.endian に対する指定として使う。
エンディアンを指定したい場合はこれらの定数を使用するべきである

```
input.bit_order = bit_order
```

デフォルトのビットオーダーを指定する構文である。
エンディアンと同じように用いる

```
config.bit_order.msb
config.bit_order.lsb
```

config.bit_order.msb は msb から読み取られることを示し、
config.bit_order.lsb は lsb から読み取られることを示す。

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
バイトアラインを強制したい場合は`強制したいバイト数*8`で指定する

```
a :Type(input.peek= true)
```

フィールドの読み込みは行うが入力位置を進めないのを示すために使用する。書き込み時は単に無視される。

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

```
config.export(..)
```

将来のために予約されている。

# メタデータ

任意の式の一部ではない`config.<name list> = <value>`や`config.<name list>(<values>..)`でビルトイン指定されていない場合メタデータ構文として扱われる。なお、ビルトインの拡張などによって互換性が失われる可能性はあるが、大規模な変更のある場合には告知を行い、
またメジャーバージョンを変更することとする。
以下は例である。

```
config.cpp.namespace = "std"
```

この構文では C++の名前空間を指定する。「C++の名前空間を指定する」としているが、その扱いはジェネレーターの実装依存とする。ジェネレーターはこれを参照して名前空間を設定することが望ましいが、行わなくても良い。また、別の意味で捉えても良いが、少なくともドキュメントなどの形で利用者に示しておくべきである。「驚き最小の原則」に従っていれば問題はないだろう。
他にも例を示してみる。

```
config.go.package = "builder"
config.rust.crate = "builder"
```

これらのように言語固有の設定の場合は`config.<lang>.<setting>`の形でジェネレーター実装者は受け入れるとよいだろう。
ジェネレータ間共通のメタデータについては[metadata](https://on-keyday.github.io/brgen/doc/docs/metadata)で述べる。
