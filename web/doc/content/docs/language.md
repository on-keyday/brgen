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

以下はユーザー向け言語仕様である。厳密な言語仕様は別途 [開発ページの spec ディレクトリ内](https://github.com/on-keyday/brgen/tree/main/spec)で定義するものとする。

## 設計思想

brgen(lang)は宣言的にバイナリフォーマットおよびそのエンコード・デコードルールを記述するための言語(DSL)である。
python のようなインデントベースの構文を採用している。
これは、`{}`を排除することで見た目的に見やすくすることを意図している。
基本は定義だけを宣言的に書けば自動で生成できるよう構文を開発するが、どう頑張っても宣言型では見ずらい部分、
また、仮に書けたとして解析が難しかったり(これは作者の力量不足もあるのだが)、あるいは効率的にできない
部分もあるため、データ保持形式とエンコード・デコードルールを別にするカスタムフォーマットというものも導入する。
カスタムフォーマットのエンコード・デコードルールは手続き型で記述される

## フォーマット

フォーマットは定義/エンコード/デコードの変換単位である。
基本的にジェネレーターは 1 フォーマット 1 単位として変換を行う。
以下はなんの定義、エンコード、デコードも行わないフォーマットである。

```
format A
  ..
```

これは以下のように変換される(疑似 C++,Buffer Overflow については考えない)

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

なお、エンディアンを明確に指定しなかった場合デフォルトのエンディアンはビッグエンディアンが推奨される
(もちろん、コマンドラインフラグなどで変更できるようにしても良いがこれはオプションである)
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

なお N には任意の自然数(N>0)が入る。
つまり、`u1`や`u24`、`u120`といった
`8*2^n(n>=0)`ビットでないものでもよい
