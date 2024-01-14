---
title: "Language"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# 言語仕様

この文書はユーザー向け言語仕様である。厳密な言語仕様は別途 [開発ページの spec ディレクトリ内](https://github.com/on-keyday/brgen/tree/main/spec)で定義するものとする。

## 設計思想

brgen(lang)は宣言的にバイナリフォーマットおよびそのエンコード・デコードルールを記述するための言語(DSL)である。
python のようなインデントベースの構文を採用している。
これは、`{}`を排除することで見た目的に見やすくすることを意図している。
基本は定義だけを宣言的に書けば自動で生成できるよう構文を開発するが、どう頑張っても宣言型では見づらい、わかりづらい部分、
また、仮に書けたとして解析が難しかったり(これは作者の力量不足もあるのだが)、あるいは効率的にできない部分もあるため、データ保持形式とエンコード・デコードルールを別にする Custom フォーマットというものも導入する。
カスタムフォーマットのエンコード・デコードルールは手続き型で記述される。

## フォーマット

フォーマットは定義/エンコード/デコードの変換単位である。
基本的にジェネレーターは 1 フォーマット 1 単位として変換を行う。
以下はなんの定義、エンコード、デコードも行わないフォーマットである。

```
format A
  ..
```

これは以下のように変換される
(以降の生成先コードは C++である。Buffer Overflow については考えない。ジェネレーターは実際には範囲チェックなどを行う処理を挿入すべきであり、以下のようなコードは脆弱性の原因となりうるので生成するべきではない。あくまで説明用である)

```cpp
struct A {
    bool encode(byte*);
    bool decode(const byte*);
};

inline bool A::encode(byte* buf) const {
    return true;
}

inline bool A::decode(const byte* buf) {
    return true;
}

```

なお、前述の Custom フォーマットに対して、宣言的な定義のみで記述されたものを As-Is フォーマットと呼ぶ。

Custom フォーマットは以下のような形式である。

```
format Custom:
    fn encode():
       ..
    fn decode():
       ..
```

encode 関数と decode 関数を定義することで Custom フォーマットとなる。
なお、どちらかしか定義しないということも可能であり、その場合は定義されなかった方は As-Is フォーマットと同じルールで解釈される。
なお、これらの処理(encode/decode 関数)の対称性、一貫性についてはプログラマーの責任とする。これは、処理の一貫性を厳密に検証しようとすると[停止性問題](https://ja.wikipedia.org/wiki/%E5%81%9C%E6%AD%A2%E6%80%A7%E5%95%8F%E9%A1%8C)になってしまうため、解析によっては検証しきれないからである。
(もちろん一部のものについては[静的コード解析](https://ja.wikipedia.org/wiki/%E9%9D%99%E7%9A%84%E3%82%B3%E3%83%BC%E3%83%89%E8%A7%A3%E6%9E%90)を作り込めば検証可能であろうが、現時点では対応していない)

パディングは自動的には挿入されない。
これは、この言語がプログラミング言語非依存であることを意図しているからである。
他にもネットワークプロトコルなどパディングを不用意に入れてほしくないものを記述の主な対象にするためパディングは 0 でありしたがって C 言語の構造体との互換性はない。

## フィールド

フィールドは定義/エンコード/デコードの最小単位である。
brgen(lang)では基本的にフィールドは定義/エンコード/デコードの処理に変換される。

例を示す。
以下は単純な 32 ビット符号なし整数型のフォーマットである。

```brgen
format A
  data :u32
```

この例では data というフィールドを定義している
以上は以下のように変換される

```cpp
struct A {
    uint32_t data;
    bool encode(byte*)const;
    bool decode(const byte*);
};

inline bool A::encode(byte* buf) const {
    *buf = (data >> 24) & 0xff;
    buf++;
    *buf = (data >> 16) & 0xff;
    buf++;
    *buf = (data >> 8) & 0xff;
    buf++;
    *buf = data & 0xff;
    return true;
}

inline bool A::decode(const byte* buf) {
    data = (uint32_t(*buf) << 24);
    buf++;
    data |= (uint32_t(*buf) << 16);
    buf++;
    data |= (uint32_t(*buf) << 8);
    buf++;
    data |= uint32_t(*buf);
    return true;
}

```

なお、型でエンディアンを明確に指定しなかった場合のデフォルトのエンディアンはビッグエンディアンとする。
(もちろん、コマンドラインフラグなどでデフォルトをリトルエンディアンなどに変更できるようにしても良いがこれはオプションである)
もし、エンディアンを明確に指定したい場合`ub32`や`ul32`というように、`u`の後に`b`(ビッグエンディアン)か`l`(リトルエンディアン)をつけることで指定することができる。
また、動的にデフォルトのエンディアンを変更する構文もある
(これはたとえば ELF など先頭のデータによってエンディアンが代わりうるときに使用できるだろう)

## 型

brgen(lang)の型は以下のようなものがある。

### 整数型

整数型は以下の 2 種類がある。

```py
sN # 符号あり整数型
uN # 符号なし整数型
```

N には任意のビット数(自然数,N>0)が入る。
つまり、`u1`や`u24`、`u120`といった
`8*2^n(n>=0)`ビットでないものでもよい

### 配列型

配列型は以下の通りである

```py
[length]type
```

これは
As-Is フォーマットでは要素数 length の type 型の配列を表す。
length が静的に決定可能ならば(厳密には加えて type が特定の要件を満たすとき)固定長配列として扱われ、そうでない場合は length 長の動的配列として解釈される。
Custom フォーマットでは length が静的に決定可能ならば As-Is フォーマットと同じ、そうでないならば
length は無視され、任意長の動的配列として解釈される。

以下に例を示す

```
format A:
    len_minus_1 :u32
    data :[len_minus_1 + 1]u8
    suffix: [3]u8
```

これは、要素数 len_minus_1 + 1 の符号なし 8bit 整数型の配列として解釈される。

これは以下のように生成されうる

```cpp
struct A {
    uint32_t len_minus_1;
    uint8_t* data;
    uint8_t suffix[3];
    bool encode(byte*)const;
    bool decode(const byte*);
};

inline bool A::encode(byte* buf) const {
    *buf = (len_minus_1 >> 24) & 0xff;
    buf++;
    *buf = (len_minus_1 >> 16) & 0xff;
    buf++;
    *buf = (len_minus_1 >> 8) & 0xff;
    buf++;
    *buf = len_minus_1 & 0xff;
    for(auto i=0;i<(len_minus_1+1);i++){
        *buf = data[i];
        buf++;
    }
    for(auto i=0;i<3;i++) {
        *buf = suffix[i];
        buf++;
    }
    return true;
}

inline bool A::decode(const byte* buf) {
    len_minus_1 = (uint32_t(*buf) << 24);
    buf++;
    len_minus_1 |= (uint32_t(*buf) << 16);
    buf++;
    len_minus_1 |= (uint32_t(*buf) << 8);
    buf++;
    len_minus_1 |= uint32_t(*buf);
    data = malloc(sizeof(data[0](len_minus_1 + 1)));
    for(auto i=0;i<(len_minus_1+1);i++) {
        data[i] = *buf;
        buf++;
    }
    for(auto i=0;i<3;i++) {
        suffix[i] = *buf;
        buf++;
    }
    return true;
}

```

なおこの例では各要素に対して for ループで回しているが、ジェネレーターは意味を変えない範囲で最適化をしてもよい。例えばこの場合は、memcpy などの関数を使うように書き換えてもよい。

### 固定バイト列型(文字列)

型部分に文字列を指定することで固定バイト列型を表現できる。

```
format A:
   magic :"magic"
```
